// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hpk_updater.h"

#include <unistd.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <utility>

#include <base/logging.h>
#include <base/time/time.h>
#include <cfm-dfu-notification/idfu_notification.h>

#include "../common/messagepack/messagepack.h"
#include "hcp.h"
#include "message_bus.h"
#include "usb.h"
#include "utils.h"

namespace huddly {

constexpr auto kDeviceWaitTimeoutMillisec = 30000;
const uint32_t kUpdateTimeoutSeconds = 480;
constexpr uint8_t kVideoInterfaceClass = 0x0e;
constexpr uint8_t kAudioInterfaceClass = 0x01;
constexpr uint8_t kControlInterfaceSubClass = 0x01;
constexpr char kCameraName[] = "Huddly IQ";

std::unique_ptr<HpkUpdater> HpkUpdater::Create(uint16_t usb_vendor_id,
                                               uint16_t usb_product_id,
                                               const std::string& usb_path) {
  auto instance = std::unique_ptr<HpkUpdater>(
      new HpkUpdater(usb_vendor_id, usb_product_id, usb_path));

  instance->usb_ = huddly::Usb::Create();
  if (!instance->usb_) {
    LOG(ERROR) << "Usb init failed";
    return nullptr;
  }

  if (!instance->Connect()) {
    LOG(ERROR) << "Connect failed";
    return nullptr;
  }

  return instance;
}

HpkUpdater::~HpkUpdater() {
  if (!hlink_) {
    return;
  }
  auto usb_device = hlink_->GetUsbDevice();
  if (!usb_device) {
    return;
  }
  ReattachMediaKernelDrivers(usb_device);
}

enum class UpdateAction {
  kNoUpdateNeeded,
  kRunUpdate,
  kRunVerification,
  kUnknown
};

static UpdateAction DetermineUpdateAction(const CameraInformation& camera_info,
                                          const HpkFile& hpk_file,
                                          bool force_update) {
  std::string error_str;
  std::string hpk_firmware_version_string;
  if (!hpk_file.GetFirmwareVersionString(&hpk_firmware_version_string,
                                         &error_str)) {
    LOG(WARNING) << "Failed to read firmware version from hpk file. "
                 << error_str;
    return UpdateAction::kUnknown;
  }

  LOG(INFO) << "Camera firmware version: " << camera_info.firmware_version;
  LOG(INFO) << "Hpk firmware version:    " << hpk_firmware_version_string;

  if (camera_info.ram_boot_selector != camera_info.boot_decision) {
    LOG(INFO) << "Verification needed. Verifying...";
    return UpdateAction::kRunVerification;
  }

  if (force_update) {
    LOG(INFO) << "Force update requested. Updating...";
    return UpdateAction::kRunUpdate;
  }

  if (hpk_firmware_version_string != camera_info.firmware_version) {
    LOG(INFO) << "Camera versions differ. Updating...";
    return UpdateAction::kRunUpdate;
  }

  LOG(INFO) << "No upgrade or verification needed.";
  return UpdateAction::kNoUpdateNeeded;
}

bool HpkUpdater::DoUpdate(base::FilePath hpk_file_path,
                          bool force,
                          bool udev_mode,
                          const std::string& configuration) {
  std::string error_str;
  const auto hpk_file = huddly::HpkFile::Create(hpk_file_path, &error_str);

  if (!hpk_file) {
    LOG(ERROR) << "Failed to read hpk file '" << hpk_file_path.MaybeAsASCII()
               << "': " << error_str;
    return false;
  }

  if (!HpkUpdateSupported()) {
    LOG(ERROR) << "The current camera firmware version, '"
               << camera_info_.firmware_version
               << "' does not support hpk update.";
    return false;
  }

  UpdateAction action = UpdateAction::kUnknown;
  const auto kNumUpdatePasses = udev_mode ? 1 : 2;
  for (int update_pass = 1; update_pass <= kNumUpdatePasses; update_pass++) {
    action = DetermineUpdateAction(camera_info_, *hpk_file, force);

    if (action == UpdateAction::kUnknown) {
      LOG(ERROR) << "Unable to determine update state";
      return false;
    }

    if (action == UpdateAction::kNoUpdateNeeded) {
      if (!ConfigureCamera(configuration)) {
        LOG(ERROR) << "Failed to configure camera";
        return false;
      }
      return true;
    }

    // This means that either an update or verification is needed.
    // The update notification will restart after each pass.
    std::unique_ptr<IDfuNotification> dfu_notification =
        IDfuNotification::For(kCameraName);
    dfu_notification->NotifyStartUpdate(kUpdateTimeoutSeconds);

    bool reboot_requested;
    if (!DoSingleUpdatePass(*hpk_file, &reboot_requested)) {
      dfu_notification->NotifyEndUpdate(false);
      return false;
    }

    if (action == UpdateAction::kRunUpdate && !reboot_requested) {
      dfu_notification->NotifyEndUpdate(false);
      LOG(ERROR)
          << "Expected reboot request after update pass, but none received";
      return false;
    }

    if (action == UpdateAction::kRunVerification && reboot_requested) {
      LOG(ERROR) << "Unexpected reboot requested after verification pass";
      dfu_notification->NotifyEndUpdate(false);
      return false;
    }

    if (reboot_requested) {
      LOG(INFO) << "Reboot requested. Rebooting...";
      if (udev_mode) {
        // Do not wait for detach in udev mode, since this is konwn to fail on
        // older kernel versions.
        if (!Reboot(false)) {
          LOG(ERROR) << "Failed to reboot";
          dfu_notification->NotifyEndUpdate(false);
          return false;
        }
      } else {
        if (!RebootAndReattach()) {
          LOG(ERROR) << "Failed to reboot";
          dfu_notification->NotifyEndUpdate(false);
          return false;
        }
      }
    }
    dfu_notification->NotifyEndUpdate(true);
  }

  if (action == UpdateAction::kRunVerification) {
    LOG(INFO) << "Update finished successfully";
    if (!ConfigureCamera(configuration)) {
      LOG(ERROR) << "Failed to configure camera";
      return false;
    }
  }
  return true;
}

bool HpkUpdater::Reboot(bool wait_for_detach) {
  if (!hlink_->SendReboot(wait_for_detach)) {
    LOG(ERROR) << "Failed to  reboot camera";
    return false;
  }
  kernel_drivers_detached_ = false;
  return true;
}

bool HpkUpdater::RebootAndReattach() {
  if (!Reboot(true)) {
    return false;
  }

  if (!Connect()) {
    LOG(ERROR) << "Connect failed";
    return false;
  }

  return true;
}

static bool FirmwareIsHuddlyIQ(const std::string& version_str) {
  const std::string kProductIdentifier("HuddlyIQ");
  return (version_str.find(kProductIdentifier) != std::string::npos);
}

bool HpkUpdater::HpkUpdateSupported() {
  // Only Huddly IQ, firmware versions 1.2.0 and later is supported.
  return (FirmwareIsHuddlyIQ(camera_info_.firmware_version) &&
          camera_info_.firmware_version_numeric >= NumericVersion({1, 2, 0}));
}

bool HpkUpdater::ErrorLogReadSupported() {
  // Only Huddly IQ, firmware versions 1.3.14 and later is supported.
  return (FirmwareIsHuddlyIQ(camera_info_.firmware_version) &&
          camera_info_.firmware_version_numeric >= NumericVersion({1, 3, 14}));
}

bool HpkUpdater::ConfigureCamera(const std::string& configuration) {
  const std::string kHangoutsMeetConfiguration("hmh");
  const std::string kJamboardConfiguration("jamboard");
  if (configuration == kHangoutsMeetConfiguration) {
    return ConfigureCameraHmh();
  } else if (configuration == kJamboardConfiguration) {
    return ConfigureCameraJamboard();
  }

  LOG(ERROR) << "Unknown configuration: " << configuration;
  return false;
}

bool HpkUpdater::ConfigureCameraHmh() {
  const uint16_t kClownfishProductId = 0x0031;
  if (usb_product_id_ != kClownfishProductId) {
    return true;
  }

  const std::string kCameraModeDualStream("dual-stream");
  if (camera_info_.camera_mode == kCameraModeDualStream) {
    LOG(INFO) << "Camera already configured";
    return true;
  }

  if (!SetCameraMode(kCameraModeDualStream)) {
    return false;
  }

  LOG(INFO) << "Camera configuration changed. Rebooting";
  Reboot(false);
  return true;
}

bool HpkUpdater::ConfigureCameraJamboard() {
  const uint16_t kBoxfishProductId = 0x0021;
  if (usb_product_id_ != kBoxfishProductId) {
    return true;
  }

  const std::string kCameraModeMjpeg("mjpeg");
  if ((camera_info_.camera_mode == kCameraModeMjpeg) &&
      (camera_info_.autozoom_enabled == false)) {
    LOG(INFO) << "Camera already configured";
    return true;
  }

  if (!SetCameraMode(kCameraModeMjpeg)) {
    return false;
  }

  if (!DisableAutozoom()) {
    return false;
  }

  LOG(INFO) << "Camera configuration changed. Rebooting";
  Reboot(false);
  return true;
}

HpkUpdater::HpkUpdater(uint16_t usb_vendor_id,
                       uint16_t usb_product_id,
                       const std::string& usb_path)
    : usb_vendor_id_(usb_vendor_id),
      usb_product_id_(usb_product_id),
      usb_path_(usb_path) {}

bool HpkUpdater::Connect() {
  auto usb_device = usb_->WaitForDevice(usb_vendor_id_, usb_product_id_,
                                        usb_path_, kDeviceWaitTimeoutMillisec);
  if (!usb_device) {
    LOG(ERROR) << "Could not find USB device with vid:pid="
               << huddly::UsbVidPidString(usb_vendor_id_, usb_product_id_);
    return false;
  }

  LOG(INFO) << "Found USB device with vid:pid="
            << huddly::UsbVidPidString(usb_vendor_id_, usb_product_id_);

  if (!usb_device->Open()) {
    LOG(ERROR) << "Failed to open device";
    return false;
  }

  if (!DetachMediaKernelDrivers(usb_device.get())) {
    LOG(ERROR) << "Unable to detach kernel driver";
    return false;
  }

  hlink_ = HLinkVsc::Create(std::move(usb_device));
  if (!hlink_) {
    LOG(ERROR) << "Failed to create HLinkVsc instance";
    return false;
  }

  if (!hlink_->Connect()) {
    LOG(ERROR) << "Failed to connect to HLinkVsc device";
    return false;
  }

  if (!GetCameraInformation(&camera_info_)) {
    LOG(WARNING) << "Failed to obtain camera information";
  }

  return true;
}

bool HpkUpdater::GetCameraInformation(CameraInformation* info) {
  if (!GetFirmwareVersion(&info->firmware_version,
                          &info->firmware_version_numeric)) {
    LOG(ERROR) << "Failed to get camera version info";
    return false;
  }

  // Figure out if we are going to verify.
  std::vector<uint8_t> product_info_msgpack;
  if (!GetProductInfo(&product_info_msgpack)) {
    return false;
  }

  auto unpacker = huddly::messagepack::Unpacker::Create(product_info_msgpack);
  if (!unpacker) {
    LOG(ERROR) << "Unable to decode product info reply";
    return false;
  }
  huddly::messagepack::Map product_info;
  if (!unpacker->GetRoot<huddly::messagepack::Map>(&product_info)) {
    LOG(ERROR) << "Failed to get product info as map";
    return false;
  }

  huddly::messagepack::Map bac_fsbl;
  if (!product_info.GetValueAs<huddly::messagepack::Map>("bac_fsbl",
                                                         &bac_fsbl)) {
    LOG(ERROR) << "Could not find bac_fsbl in product info";
    return false;
  }

  if (!bac_fsbl.GetValueAs<std::string>("ram_boot_selector",
                                        &info->ram_boot_selector)) {
    LOG(ERROR) << "Could not find ram_boot_selector";
    return false;
  }

  if (!bac_fsbl.GetValueAs<std::string>("boot_decision",
                                        &info->boot_decision)) {
    LOG(ERROR) << "Could not find boot_decision";
    return false;
  }

  product_info.GetValueAs<std::string>(kCameraModeKey, &info->camera_mode);
  product_info.GetValueAs<bool>(kAutozoomEnableKey, &info->autozoom_enabled);

  if (!product_info.GetValueAs<std::string>(kResetCauseKey,
                                            &info->reset_cause)) {
    LOG(WARNING) << "Failed to get reset cause";
    info->reset_cause = "Unknown";
  }

  return true;
}

bool HpkUpdater::GetProductInfo(std::vector<uint8_t>* product_info_msgpack) {
  const std::string kMessageName("prodinfo/get_msgpack");
  const std::string kSubscription(kMessageName + "_reply");

  const auto scoped_subscription =
      message_bus::ScopedSubscribe::Create(hlink_.get(), kSubscription);
  if (!scoped_subscription) {
    LOG(ERROR) << "Failed to subscribe to '" << kSubscription << "'";
    return false;
  }

  if (!hlink_->Send(kMessageName, nullptr, 0)) {
    LOG(ERROR) << "Failed to send '" << kMessageName << "' message";
    return false;
  }

  HLinkBuffer buffer;
  if (!hlink_->Receive(&buffer)) {
    return false;
  }

  if (buffer.GetMessageName() != kSubscription) {
    LOG(ERROR) << "Received unexpected message: '" << buffer.GetMessageName()
               << "'";
    return false;
  }

  *product_info_msgpack = buffer.GetPayload();
  return true;
}

bool HpkUpdater::SetProductInfo(
    const std::vector<uint8_t>& product_info_msgpack) {
  const std::string kMessageName("prodinfo/set_msgpack");
  const std::string kSubscription(kMessageName + "_reply");

  const auto scoped_subscription =
      message_bus::ScopedSubscribe::Create(hlink_.get(), kSubscription);
  if (!scoped_subscription) {
    LOG(ERROR) << "Failed to subscribe to '" << kSubscription << "'";
    return false;
  }

  if (!hlink_->Send(kMessageName, product_info_msgpack.data(),
                    product_info_msgpack.size())) {
    LOG(ERROR) << "Failed to send '" << kMessageName << "' message";
    return false;
  }

  HLinkBuffer buffer;
  if (!hlink_->Receive(&buffer)) {
    return false;
  }

  if (buffer.GetMessageName() != kSubscription) {
    LOG(ERROR) << "Received unexpected message: '" << buffer.GetMessageName()
               << "'";
    return false;
  }
  const auto payload = buffer.GetPayload();
  if (payload.size() != 1) {
    LOG(ERROR) << "Unexpected reply length";
    return false;
  }
  if (payload[0] != 0) {
    LOG(ERROR) << "Set product info failed";
    return false;
  }
  return true;
}

bool HpkUpdater::SetProductInfoString(const std::string& key,
                                      const std::string& value) {
  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);
  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  msgpack_pack_map(&pk, 1);
  msgpack_pack_str(&pk, key.size());
  msgpack_pack_str_body(&pk, key.c_str(), key.size());
  msgpack_pack_str(&pk, value.size());
  msgpack_pack_str_body(&pk, value.c_str(), value.size());

  std::vector<uint8_t> product_info(sbuf.data, sbuf.data + sbuf.size);
  if (!SetProductInfo(product_info)) {
    return false;
  }
  return true;
}

bool HpkUpdater::SetProductInfoBool(const std::string& key, bool value) {
  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);
  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  msgpack_pack_map(&pk, 1);
  msgpack_pack_str(&pk, key.size());
  msgpack_pack_str_body(&pk, key.c_str(), key.size());
  if (value) {
    msgpack_pack_true(&pk);
  } else {
    msgpack_pack_false(&pk);
  }

  std::vector<uint8_t> product_info(sbuf.data, sbuf.data + sbuf.size);
  if (!SetProductInfo(product_info)) {
    return false;
  }
  return true;
}

bool HpkUpdater::SetCameraMode(const std::string& mode) {
  VLOG(1) << "Setting camera mode to '" << mode << "'";
  if (!SetProductInfoString(kCameraModeKey, mode)) {
    LOG(ERROR) << "Failed to set camera mode '" << mode << "'";
    return false;
  }

  return true;
}

bool HpkUpdater::DisableAutozoom() {
  VLOG(1) << "Disabling auto-zoom";
  if (!SetProductInfoBool(kAutozoomEnableKey, false)) {
    LOG(ERROR) << "Failed to disable auto-zoom";
    return false;
  }
  return true;
}

bool HpkUpdater::GetFirmwareVersion(std::string* version,
                                    NumericVersion* version_numeric) {
  *version_numeric = NumericVersion{};

  std::vector<uint8_t> product_info_msgpack;
  if (!GetProductInfo(&product_info_msgpack)) {
    return false;
  }

  auto unpacker = messagepack::Unpacker::Create(product_info_msgpack);
  if (!unpacker) {
    LOG(ERROR) << "Unable to decode product info reply";
    return false;
  }
  messagepack::Map product_info;
  if (!unpacker->GetRoot<messagepack::Map>(&product_info)) {
    LOG(ERROR) << "Failed to get product info as map";
    return false;
  }

  if (!product_info.GetValueAs<std::string>("app_version", version)) {
    LOG(ERROR) << "Could not find firmwave version";
    return false;
  }

  messagepack::Map build_data;
  if (!product_info.GetValue<messagepack::Map>("build_data", &build_data)) {
    LOG(ERROR) << "Failed to read build data";
    return false;
  }
  messagepack::Array version_array;
  if (!build_data.GetValue<messagepack::Array>("version", &version_array)) {
    LOG(ERROR) << "Failed to get firmware version as array";
    return false;
  }
  const auto kVersionLength = 3;
  if (version_array.Size() < kVersionLength) {
    LOG(ERROR) << "Version has wrong format";
    return false;
  }
  std::vector<int64_t> values;
  if (!version_array.GetValuesAs<int64_t>(&values)) {
    LOG(ERROR) << "Could not get version as array of signed integers";
    return false;
  }
  std::vector<uint32_t> temp_version_numeric;
  for (const auto v : values) {
    if (v < 0 || v > std::numeric_limits<uint32_t>::max()) {
      LOG(ERROR) << "Numeric version element with value" << v
                 << " out of range";
      return false;
    }
    temp_version_numeric.push_back(v);
  }
  *version_numeric = NumericVersion(temp_version_numeric);

  return true;
}

std::string HpkUpdater::GetResetCause() {
  return camera_info_.reset_cause;
}

bool HpkUpdater::ReadErrorLog(std::string* log) {
  log->clear();
  if (!ErrorLogReadSupported()) {
    LOG(ERROR) << "Error log operations not supported on this version";
    return false;
  }

  const std::string kCommand("error_logger/read_simple");
  const std::string kSubscription(kCommand + "_reply");
  const auto scoped_subscription =
      message_bus::ScopedSubscribe::Create(hlink_.get(), kSubscription);
  if (!scoped_subscription) {
    LOG(ERROR) << "Failed to subscribe to " << kSubscription;
    return false;
  }

  if (!hlink_->Send(kCommand, nullptr, 0)) {
    LOG(ERROR) << "Failed to send '" << kCommand << "' command";
    return false;
  }

  huddly::HLinkBuffer hl_buffer;
  if (!hlink_->Receive(&hl_buffer)) {
    LOG(ERROR) << "Failed to receive packet";
    return false;
  }

  auto unpacker = messagepack::Unpacker::Create(hl_buffer.GetPayload());
  if (!unpacker) {
    LOG(ERROR) << "Unable to decode status message";
    return false;
  }

  messagepack::Map error_log;
  if (!unpacker->GetRoot<messagepack::Map>(&error_log)) {
    LOG(ERROR) << "Failed to get upgrader status as map.";
    return false;
  }

  int64_t status;
  if (!error_log.GetValueAs<int64_t>("error", &status)) {
    LOG(ERROR) << "Read error log failed. No 'error' field in map";
    return false;
  }

  if (status != 0) {
    LOG(ERROR) << "Read error log failed on device. Status code: " << status;
    return false;
  }

  messagepack::Bin bin;
  if (!error_log.GetValue<messagepack::Bin>("log", &bin)) {
    LOG(ERROR) << "Read error log failed. No 'log' field in map";
    return false;
  }
  std::vector<uint8_t> as_vector;
  if (!bin.GetValues(&as_vector)) {
    LOG(ERROR) << "Failed to get error log data as byte array";
    return false;
  }

  std::string raw_log;
  // Copy to string, but remove '\0' characters.
  std::copy_if(std::cbegin(as_vector), std::cend(as_vector),
               std::back_inserter(raw_log),
               [](const uint8_t c) { return c != '\0'; });

  // Strip ANSI SGR codes on a line-by line basis to avoid losing parts of
  // the log if there are incomplete ANSI codes in the middle of the log.
  std::istringstream iss(raw_log);
  std::string line;
  while (getline(iss, line)) {
    *log += StripAnsiSgrCodes(line) += '\n';
  }
  log->pop_back();  // Remove extra '\n'.

  return true;
}

bool HpkUpdater::EraseErrorLog() {
  if (!ErrorLogReadSupported()) {
    LOG(ERROR) << "Error log operations not supported on this version";
    return false;
  }

  const std::string kCommand("error_logger/erase");

  const std::string kStartedSubscription(kCommand + "_started");
  const auto started_scoped_subscription =
      message_bus::ScopedSubscribe::Create(hlink_.get(), kStartedSubscription);
  if (!started_scoped_subscription) {
    LOG(ERROR) << "Failed to subscribe to " << kStartedSubscription;
    return false;
  }

  const std::string kDoneSubscription(kCommand + "_done");
  const auto done_scoped_subscription =
      message_bus::ScopedSubscribe::Create(hlink_.get(), kDoneSubscription);
  if (!done_scoped_subscription) {
    LOG(ERROR) << "Failed to subscribe to " << kDoneSubscription;
    return false;
  }

  if (!hlink_->Send(kCommand, nullptr, 0)) {
    LOG(ERROR) << "Failed to send '" << kCommand << "' command";
    return false;
  }

  const auto timeout_ms = 30'000;
  const auto start = base::TimeTicks::Now();
  bool erase_done = false;
  while (!erase_done) {
    huddly::HLinkBuffer hl_buffer;
    if (!hlink_->Receive(&hl_buffer)) {
      LOG(ERROR) << "Failed to receive error log status packet";
      return false;
    }

    const auto message_name = hl_buffer.GetMessageName();
    if (message_name == kStartedSubscription) {
      VLOG(1) << "Error log erase started";
    }
    if (message_name == kDoneSubscription) {
      VLOG(1) << "Error log erase done";
      // This message simply contains a 32 bit int as status value. No need to
      // unpack as messagepack.
      const auto status =
          *reinterpret_cast<const int32_t*>(hl_buffer.GetPayload().data());
      if (status != 0) {
        LOG(WARNING) << "Error log erase failed";
        return false;
      }
      erase_done = true;
    }

    if ((base::TimeTicks::Now() - start).InMilliseconds() > timeout_ms) {
      LOG(ERROR) << "Error log erase timed out after " << timeout_ms << "ms.";
      return false;
    }
  }

  return true;
}

class UpgradeStatusHandler {
 public:
  // Returns false until finished, then true.
  // Additional status information is communicated through the function
  // object.
  bool operator()(std::vector<uint8_t> payload) {
    auto unpacker = messagepack::Unpacker::Create(payload);
    if (!unpacker) {
      LOG(ERROR) << "Unable to decode status message";
      return false;
    }
    messagepack::Map upgrader_status;
    if (!unpacker->GetRoot<messagepack::Map>(&upgrader_status)) {
      LOG(ERROR) << "Failed to get upgrader status as map.";
      return false;
    }

    last_status_ = upgrader_status.ToString();
    VLOG(2) << "Status: " << last_status_;

    std::string operation;
    if (!upgrader_status.GetValue<std::string>("operation", &operation)) {
      LOG(WARNING) << "No operation field in status message";
      operation = "Unknown";
    }

    int64_t error = 0;
    upgrader_status.GetValueAs<int64_t>("error", &error);
    if (error) {
      return true;
    }

    upgrader_status.GetValueAs<double>("total_points", &total_points_);
    upgrader_status.GetValueAs<double>("elapsed_points", &elapsed_points_);

    std::unique_ptr<IDfuNotification> dfu_notification =
        IDfuNotification::For(kCameraName);
    double elapsed_fraction = elapsed_points_ / total_points_;
    double elapsed_percentage = 100.0 * elapsed_fraction;
    if (elapsed_percentage - previous_reported_elapsed_percentage_ >= 5.0) {
      dfu_notification->NotifyUpdateProgress(elapsed_fraction);
      LOG(INFO) << "Progress: " << std::round(elapsed_percentage) << "%";
      previous_reported_elapsed_percentage_ = elapsed_percentage;
    }

    if (operation == "done") {
      LOG(INFO) << "Done!";
      if (upgrader_status.GetValueAs<int64_t>("error_count", &error_count_)) {
        if (error_count_ == 0) {
          success_ = true;
        }
      }

      upgrader_status.GetValue<bool>("reboot", &reboot_needed_);

      return true;
    }

    return false;
  }

  bool success() const { return success_; }
  bool reboot_needed() const { return reboot_needed_; }
  const std::string& last_status() { return last_status_; }

 private:
  bool success_ = false;
  std::string last_status_;
  bool reboot_needed_ = false;
  int64_t error_count_ = -1;

  double total_points_ = 0.0;
  double elapsed_points_ = 0.0;
  double previous_reported_elapsed_percentage_ = 0.0;
};

bool HpkUpdater::DoSingleUpdatePass(const HpkFile& hpk_file,
                                    bool* reboot_needed) {
  VLOG(1) << "Send .hpk file";
  if (!hcp::Write(hlink_.get(), "upgrade.hpk", hpk_file.GetContents())) {
    LOG(ERROR) << "Failed to send .hpk file";
    return false;
  }

  VLOG(1) << "File uploaded successfully";

  UpgradeStatusHandler status_handler;
  if (!HpkRun("upgrade.hpk", std::ref(status_handler))) {
    LOG(ERROR) << "hpk/run failed";
    return false;
  }

  if (!status_handler.success()) {
    LOG(ERROR) << "Upgrade failed. Last status message: "
               << status_handler.last_status();
    return false;
  }

  *reboot_needed = status_handler.reboot_needed();
  return true;
}

bool HpkUpdater::HpkRun(
    const std::string& filename,
    std::function<bool(std::vector<uint8_t>)> status_handler) {
  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);

  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

  msgpack_pack_map(&pk, 1);
  msgpack_pack_str(&pk, 8);
  msgpack_pack_str_body(&pk, "filename", 8);
  msgpack_pack_str(&pk, filename.size());
  msgpack_pack_str_body(&pk, filename.c_str(), filename.size());

  const std::string kSubscription("upgrader/status");
  const auto scoped_subscription =
      message_bus::ScopedSubscribe::Create(hlink_.get(), kSubscription);
  if (!scoped_subscription) {
    LOG(ERROR) << "Failed to subscribe to '" << kSubscription << "'";
    return false;
  }

  if (!hlink_->Send("hpk/run", reinterpret_cast<uint8_t*>(sbuf.data),
                    sbuf.size)) {
    LOG(ERROR) << "hpk/run failed";
    return false;
  }

  while (1) {
    huddly::HLinkBuffer hl_buffer;
    if (!hlink_->Receive(&hl_buffer)) {
      LOG(ERROR) << "Failed to receive packet";
      return false;
    }
    std::vector<uint8_t> packet = hl_buffer.CreatePacket();
    const bool finished = status_handler(hl_buffer.GetPayload());
    if (finished) {
      break;
    }
  }

  return true;
}

bool HpkUpdater::IterateMediaKernelDrivers(
    UsbDevice* usb_device,
    std::function<void(const libusb_interface interface)> all_drivers) {
  if (!usb_device) {
    LOG(ERROR) << "Failed to obtain usb device";
    return false;
  }

  auto config = usb_device->GetActiveConfigDescriptor();

  if (!config) {
    LOG(ERROR) << "Failed to get active configuration descriptor";
    return false;
  }

  const int num_interfaces = config->bNumInterfaces;

  const auto interface_begin = config->interface;
  const auto interface_end = config->interface + num_interfaces;
  std::for_each(interface_begin, interface_end, all_drivers);
  return true;
}

bool HpkUpdater::DetachMediaKernelDrivers(UsbDevice* usb_device) {
  return IterateMediaKernelDrivers(
      usb_device, [this, usb_device](const libusb_interface interface) {
        const auto interface_descr = &interface.altsetting[0];
        const int interface_num = interface_descr->bInterfaceNumber;
        bool driver_was_active = false;
        if (interface_descr->bInterfaceClass != kVideoInterfaceClass &&
            interface_descr->bInterfaceClass != kAudioInterfaceClass) {
          return;
        }
        if (!usb_device->CheckKernelDriverActive(interface_num,
                                                 &driver_was_active)) {
          LOG(WARNING)
              << "Unable to determine state of kernel driver at interface:"
              << interface_num;
          return;
        }

        if (driver_was_active &&
            !usb_device->DetachKernelDriver(interface_num)) {
          LOG(ERROR) << "Unable to detach active kernel driver for interface:"
                     << interface_num;
          return;
        }

        // We must detach and claim ownership of the control interfaces to
        // prevent another service from reclaiming it.
        if (interface_descr->bInterfaceSubClass != kControlInterfaceSubClass) {
          return;
        }

        // Claim ownership of the controllers
        if (!usb_device->ClaimInterface(interface_num)) {
          LOG(ERROR) << "Failed to claim interface " << interface_num;
          return;
        }
        kernel_drivers_detached_ = true;
      });
}

bool HpkUpdater::ReattachMediaKernelDrivers(UsbDevice* usb_device) {
  if (!kernel_drivers_detached_) {
    return true;
  }
  return IterateMediaKernelDrivers(
      usb_device, [usb_device](const libusb_interface interface) {
        const auto interface_descr = &interface.altsetting[0];
        const int interface_num = interface_descr->bInterfaceNumber;
        if ((interface_descr->bInterfaceClass != kVideoInterfaceClass &&
             interface_descr->bInterfaceClass != kAudioInterfaceClass)
            // We only need to reattach the controller classes as the rest will
            // follow
            ||
            interface_descr->bInterfaceSubClass != kControlInterfaceSubClass) {
          return;
        }

        if (!usb_device->ReleaseInterface(interface_num)) {
          LOG(WARNING) << "Failed to release interface " << interface_num;
          return;
        }

        if (!usb_device->ReattachKernelDriver(interface_num)) {
          LOG(ERROR) << "Unable to reattach active kernel driver for interface:"
                     << interface_num;
          return;
        }
      });
}

}  // namespace huddly
