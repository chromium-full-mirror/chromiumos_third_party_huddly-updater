// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>

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

}  // namespace

Flasher::Flasher(const Firmware& firmware, bool forceful_upgrade, bool dry_run)
    : firmware_(firmware),
      minidev_(new MinicamDevice(kVendorId, kProductIdBootloader)),
      forceful_upgrade_(forceful_upgrade),
      dry_run_(dry_run),
      is_eligible_to_upgrade_app_(false),
      is_eligible_to_upgrade_boot_(false) {}

Flasher::~Flasher() {
  std::string err_msg;
  if (!minidev_->Teardown(&err_msg)) {
    err_msg += ".. failed to tear down the minidev Flasher used";
    LOG(ERROR) << err_msg;
  }
}

bool Flasher::Init(std::string* err_msg) {
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
  MinicamDevice minidev_app(kVendorId, kProductIdApp);
  if (!minidev_app.CheckIfExists()) {
    *err_msg += ".. camera not detected: ";
    return false;
  }

  LOG(INFO) << ".. camera detected: " << minidev_app.GetId();

  StorePeripheralInfo();  // Invoke while in App mode.
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
  LOG(INFO) << "Upgrade plan: " << BoolToYesNo(IsEligibleForUpgrade())
            << std::endl
            << "  [" << BoolToYesNo(is_eligible_to_upgrade_boot_)
            << "] Bootloader " << std::setw(5) << pl_bootloader_ver_ << " -> "
            << std::setw(5) << fw_bootloader_ver_ << std::endl
            << "  [" << BoolToYesNo(is_eligible_to_upgrade_app_)
            << "] App        " << std::setw(5) << pl_app_ver_ << " -> "
            << std::setw(5) << fw_app_ver_;
}

bool Flasher::FlashAll(std::string* err_msg) const {
  if (!minidev_->Setup(err_msg)) {
    *err_msg += ".. failed to flash app in minidev setting up";
    return false;
  }

  if (is_eligible_to_upgrade_boot_) {
    LOG(INFO) << "Upgrading Bootloader: " << pl_bootloader_ver_ << " -> "
              << fw_bootloader_ver_;
    if (!FlashWithFile(firmware_.bootloader_path(), err_msg)) {
      *err_msg += ".. failed to upgrade bootloader";
    }
  }

  if (is_eligible_to_upgrade_app_) {
    LOG(INFO) << "Upgrading App: " << pl_app_ver_ << " -> " << fw_app_ver_;
    if (!FlashWithFile(firmware_.app_path(), err_msg)) {
      *err_msg += ".. failed to upgrade app";
    }
  }

  if (!minidev_->Teardown(err_msg)) {
    *err_msg += ".. failed to tear down the minidev Flasher used";
    return false;
  }
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
  firmware_.ParseManifest(&fw_app_ver_, &fw_bootloader_ver_, &fw_hw_rev_);
}

void Flasher::StorePeripheralInfo() {
  // This special routine is a workaround:
  // The App version can be queries only in the App mode.
  MinicamDevice minidev_app(kVendorId, kProductIdApp);

  std::string err_msg;
  minidev_app.Setup(&err_msg);
  minidev_app.GetVersion(&pl_app_ver_, &pl_bootloader_ver_);
  minidev_app.GetHwRevision(&pl_hw_rev_, &err_msg);

  return;
}

}  // namespace huddly
