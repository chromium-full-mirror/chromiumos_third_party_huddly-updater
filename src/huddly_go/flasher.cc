// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <cfm-dfu-notification/idfu_notification.h>

#include <iomanip>
#include <memory>
#include <string>

#include "firmware.h"
#include "flasher.h"
#include "minicam_device.h"
#include "tools.h"

namespace huddly {

namespace {

std::string BoolToYesNo(bool test) {
  return test ? "Y" : "N";
}
const char kHuddlyGoName[] = "Huddly Go";

}  // namespace

Flasher::Flasher(const Firmware &firmware, bool forceful_upgrade, bool dry_run,
                 std::string serial_number)
    : firmware_(firmware),
      minidev_(new MinicamDevice(kVendorId, kProductIdBootloader, "",
                                 serial_number)),
      forceful_upgrade_(forceful_upgrade), dry_run_(dry_run),
      is_eligible_to_upgrade_app_(false), is_eligible_to_upgrade_boot_(false) {}

Flasher::~Flasher() {
  std::string err_msg;
  if (!minidev_->Teardown(&err_msg)) {
    err_msg += ".. failed to tear down the minidev Flasher used";
    LOG(ERROR) << err_msg;
  }
}

bool Flasher::Init(std::string *err_msg, const std::string &usb_path) {
  // Prepare the firmware package/images.
  if (!firmware_.IsReady(err_msg)) {
    *err_msg += ".. Firmware is not ready";
    return false;
  }

  if (forceful_upgrade_) {
    is_eligible_to_upgrade_app_ = true;
    is_eligible_to_upgrade_boot_ = true;
    return true;
  }

  // Object of huddly camera in App mode.
  MinicamDevice minidev_app(kVendorId, kProductIdApp, usb_path,
                            minidev_->GetSerialNumber());
  if (!minidev_app.CheckIfExists()) {
    *err_msg += ".. camera not detected: ";
    return false;
  }

  LOG(INFO) << ".. camera detected: " << minidev_app.GetId();

  // Invoke while in App mode.
  if (!StorePeripheralInfo(err_msg, usb_path)) {
    *err_msg += ".. failed to store peripheral info";
    return false;
  }
  StoreFirmwareInfo();

  // TODO(porce): When fw_hw_rev is a set, See if pl_hw_rev is a member of
  // fw_hw_rev.
  if (fw_hw_rev_ != pl_hw_rev_) {
    // Hardware revision mismatch. Do not upgrade.
    is_eligible_to_upgrade_app_ = false;
    is_eligible_to_upgrade_boot_ = false;
  } else {
    is_eligible_to_upgrade_app_ = (fw_app_ver_ != pl_app_ver_);
    is_eligible_to_upgrade_boot_ = (fw_bootloader_ver_ != pl_bootloader_ver_);
  }

  return true;
}

void Flasher::ShowUpgradeEligibility() const {
  // For human eyes in interactive shell.
  // See GetUpgradeEligibilityMessaage() for a single line.
  LOG(INFO) << "Upgrade plan: " << BoolToYesNo(IsEligibleForUpgrade())
            << std::endl
            << "  [" << BoolToYesNo(is_eligible_to_upgrade_boot_)
            << "] Bootloader " << std::setw(5) << pl_bootloader_ver_ << " -> "
            << std::setw(5) << fw_bootloader_ver_ << std::endl
            << "  [" << BoolToYesNo(is_eligible_to_upgrade_app_)
            << "] App        " << std::setw(5) << pl_app_ver_ << " -> "
            << std::setw(5) << fw_app_ver_;
}

std::string Flasher::GetUpgradeEligibilityMessage() const {
  // For log line. See ShowUpgradeEligibility() for multilines.
  return ".. Upgrade plan: " + BoolToYesNo(IsEligibleForUpgrade()) +
         " [Bootloader] " + pl_bootloader_ver_ + " -> " + fw_bootloader_ver_ +
         " [App] " + pl_app_ver_ + " -> " + fw_app_ver_;
}

bool Flasher::FlashAll(std::string* err_msg) const {
  if (!minidev_->Setup(err_msg)) {
    *err_msg += ".. failed to flash app in minidev setting up";
    return false;
  }
  std::unique_ptr<IDfuNotification> dfu_notification =
      IDfuNotification::For(kHuddlyGoName);
  dfu_notification->NotifyStartUpdate();

  if (is_eligible_to_upgrade_boot_) {
    LOG(INFO) << "Upgrading Bootloader: " << pl_bootloader_ver_ << " -> "
              << fw_bootloader_ver_;
    if (!FlashWithFile(firmware_.bootloader_path(), err_msg)) {
      *err_msg += ".. failed to upgrade bootloader";
      dfu_notification->NotifyEndUpdate(false);
      return false;
    }
  }

  if (is_eligible_to_upgrade_app_) {
    LOG(INFO) << "Upgrading App: " << pl_app_ver_ << " -> " << fw_app_ver_;
    if (!FlashWithFile(firmware_.app_path(), err_msg)) {
      *err_msg += ".. failed to upgrade app";
      dfu_notification->NotifyEndUpdate(false);
      return false;
    }
  }

  if (!minidev_->Teardown(err_msg)) {
    dfu_notification->NotifyEndUpdate(false);
    *err_msg += ".. failed to tear down the minidev Flasher used";
    return false;
  }
  dfu_notification->NotifyEndUpdate(true);
  return true;
}

bool Flasher::FlashWithFile(const std::string& file_path,
                            std::string* err_msg) const {
  uint32_t file_size;
  if (!GetFileSize(file_path, &file_size, err_msg)) {
    *err_msg += ".. failed to flash app. unknown file size";
    return false;
  }

  // TODO(porce): Consider vector<>.
  std::unique_ptr<uint8_t[]> data(new uint8_t[file_size]);

  // TODO(crbug.com/719567): Use base::ReadFile.
  uint32_t data_len =
      ReadFileToArray(file_path, file_size, data.get(), err_msg);

  if (data_len != file_size) {
    char msg[100];
    snprintf(msg, sizeof(msg),
             ".. failed to load the file contents to memory.(%d/%d) loaded. "
             "cancel flashing file: ",
             data_len, file_size);
    *err_msg += msg;
    *err_msg += file_path;
    return false;
  }

  if (!minidev_->WriteImage(data_len, data.get(), err_msg, dry_run_)) {
    *err_msg += ".. failed to write image: ";
    *err_msg += file_path;
    return false;
  }
  return true;
}

void Flasher::StoreFirmwareInfo() {
  firmware_.ParseManifestJSON(&fw_app_ver_, &fw_bootloader_ver_, &fw_hw_rev_);
}

bool Flasher::StorePeripheralInfo(std::string *err_msg,
                                  const std::string &usb_path) {
  // This special routine is a workaround:
  // The App version can be queries only in the App mode.
  MinicamDevice minidev_app(kVendorId, kProductIdApp, usb_path,
                            minidev_->GetSerialNumber());

  if (!minidev_app.Setup(err_msg)) {
    *err_msg += ".. failed to setup minidev: ";
    return false;
  }
  if (!minidev_app.GetVersion(&pl_app_ver_, &pl_bootloader_ver_)) {
    *err_msg += ".. failed to get app version: ";
    return false;
  }
  if (!minidev_app.GetHwRevision(&pl_hw_rev_, err_msg)) {
    *err_msg += ".. failed to get hw revision: ";
    return false;
  }

  return true;
}

}  // namespace huddly
