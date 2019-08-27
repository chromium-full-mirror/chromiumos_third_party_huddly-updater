// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hpk_updater.h"

#include <unistd.h>
#include <base/memory/ptr_util.h>
#include <utility>
#include <regex.h>

#include "../common/messagepack/messagepack.h"
#include "hcp.h"
#include "message_bus.h"
#include "usb.h"
#include "utils.h"

namespace huddly {

constexpr auto kDeviceWaitTimeoutMillisec = 30000;

std::unique_ptr<HpkUpdater> HpkUpdater::Create(uint16_t usb_vendor_id,
                                               uint16_t usb_product_id) {
  auto instance =
      base::WrapUnique(new HpkUpdater(usb_vendor_id, usb_product_id));

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
                          bool udev_mode) {
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
      if (!ConfigureCamera()) {
        LOG(ERROR) << "Failed to configure camera";
        return false;
      }
      return true;
    }

    bool reboot_requested;
    if (!DoSingleUpdatePass(*hpk_file, &reboot_requested)) {
      return false;
    }

    if (action == UpdateAction::kRunUpdate && !reboot_requested) {
      LOG(ERROR)
          << "Expected reboot request after update pass, but none received";
      return false;
    }

    if (action == UpdateAction::kRunVerification && reboot_requested) {
      LOG(ERROR) << "Unexpected reboot requested after verification pass";
      return false;
    }

    if (reboot_requested) {
      LOG(INFO) << "Reboot requested. Rebooting...";
      if (udev_mode) {
        // Do not wait for detach in udev mode, since this is konwn to fail on
        // older kernel versions.
        if (!Reboot(false)) {
          LOG(ERROR) << "Failed to reboot";
          return false;
        }
      } else {
        if (!RebootAndReattach()) {
          LOG(ERROR) << "Failed to reboot";
          return false;
        }
      }
    }
  }

  if (action == UpdateAction::kRunVerification) {
    LOG(INFO) << "Update finished successfully";
    if (!ConfigureCamera()) {
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

// For Huddly IQ, the hpk-updater is not supported for firmware versions
// before 1.2.0.
bool HpkUpdater::HpkUpdateSupported() {
  // This limitation is only valid for Huddly IQ. Make sure we are indeed
  // attempting to update a Huddly IQ.
  std::string& version_str = camera_info_.firmware_version;

  const std::string kProductIdentifier("HuddlyIQ");
  if (version_str.find(kProductIdentifier) == std::string::npos) {
    return true;
  }

  const int kMaxGroups = 4;  // 3 digit groups + overall match
  regmatch_t groups[kMaxGroups];
  std::string regex_string =
      R"(HuddlyIQ-([[:digit:]]+)\.([[:digit:]]+)\.([[:digit:]]+))";
  regex_t regex;
  if (regcomp(&regex, regex_string.c_str(), REG_EXTENDED)) {
    LOG(ERROR) << "Failed to compile regex";
    return true;  // It is better to assume hpk update supported in this case.
                  // It will fail soon enough if it is not.
  }

  if (regexec(&regex, version_str.c_str(), kMaxGroups, groups, 0)) {
    LOG(ERROR) << "Failed to execute regex";
    return true;  // It is better to assume hpk update supported in this case.
                  // It will fail soon enough if it is not.
  }

  std::vector<uint32_t> version;
  for (auto i = 1; i < kMaxGroups; i++) {
    int start = groups[i].rm_so;
    int end = groups[i].rm_eo;
    version.push_back(std::stoul(version_str.substr(start, end - start)));
  }

  const std::vector<uint32_t> kMinimumSupportedVersion{1, 2, 0};
  if (VersionCompare(version, kMinimumSupportedVersion) < 0) {
    return false;
  }

  return true;
}

bool HpkUpdater::ConfigureCamera() {
  const uint16_t kClownfishProductId = 0x0031;
  if (usb_product_id_ == kClownfishProductId) {
    const std::string kCameraModeDualStream("dual-stream");
    if (camera_info_.camera_mode == kCameraModeDualStream) {
      LOG(INFO) << "Camera already configured";
      return true;
    }

    LOG(INFO) << "Setting dual stream mode";
    if (!SetCameraMode(kCameraModeDualStream)) {
      LOG(ERROR) << "Failed to set dual stream mode";
      return false;
    }

    Reboot(false);
  }

  return true;
}

HpkUpdater::HpkUpdater(uint16_t usb_vendor_id, uint16_t usb_product_id)
    : usb_vendor_id_(usb_vendor_id), usb_product_id_(usb_product_id) {}

bool HpkUpdater::Connect() {
  auto usb_device = usb_->WaitForDevice(usb_vendor_id_, usb_product_id_,
                                        kDeviceWaitTimeoutMillisec);
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
  if (!GetFirmwareVersion(&info->firmware_version)) {
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

  product_info.GetValueAs<std::string>("camera-mode", &info->camera_mode);

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

bool HpkUpdater::SetCameraMode(const std::string& mode) {
  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);

  const std::string key("camera-mode");

  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  msgpack_pack_map(&pk, 1);
  msgpack_pack_str(&pk, key.size());
  msgpack_pack_str_body(&pk, key.c_str(), key.size());
  msgpack_pack_str(&pk, mode.size());
  msgpack_pack_str_body(&pk, mode.c_str(), mode.size());

  std::vector<uint8_t> product_info(sbuf.data, sbuf.data + sbuf.size);
  if (!SetProductInfo(product_info)) {
    LOG(ERROR) << "Failed to set dual stream mode";
    return false;
  }

  return true;
}

bool HpkUpdater::GetFirmwareVersion(std::string* version) {
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

    double elapsed_percentage = 100.0 * elapsed_points_ / total_points_;
    if (elapsed_percentage - previous_reported_elapsed_percentage_ >= 5.0) {
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

}  // namespace huddly
