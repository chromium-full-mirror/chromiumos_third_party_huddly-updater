// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <string>

#include "firmware.h"
#include "flasher.h"
#include "minicam_device.h"
#include "tools.h"

namespace huddly {

Flasher::Flasher(const Firmware& firmware,
                 MinicamDevice* minidev,
                 bool forceful_upgrade,
                 bool dry_run)
    : firmware_(firmware),
      minidev_(minidev),
      forceful_upgrade_(forceful_upgrade),
      dry_run_(dry_run) {}

Flasher::~Flasher() {
  std::string err_msg;
  if (!minidev_->Teardown(&err_msg)) {
    err_msg += ".. failed to tear down the minidev Flasher used";
    std::cout << err_msg << std::endl;
  }
}

bool Flasher::FlashAll(std::string* err_msg) const {
  if (!minidev_->Setup(err_msg)) {
    *err_msg += ".. failed to flash app in minidev setting up";
    return false;
  }

  bool to_upgrade_app;
  bool to_upgrade_bootloader;
  GetEligibility(&to_upgrade_app, &to_upgrade_bootloader);

  printf(
      "Upgrade plan:\n"
      "  [%s] Bootloader\n"
      "  [%s] App\n",
      to_upgrade_bootloader ? "Y" : "N", to_upgrade_app ? "Y" : "N");

  if (to_upgrade_bootloader) {
    std::cout << "Upgrading Bootloader: " << pl_bootloader_ver_ << " -> "
              << fw_bootloader_ver_ << std::endl;
    if (!FlashWithFile(firmware_.bootloader_path(), err_msg)) {
      *err_msg += ".. failed to upgrade bootloader";
    }
  }

  if (to_upgrade_app) {
    std::cout << "Upgrading App: " << pl_app_ver_ << " -> " << fw_app_ver_
              << std::endl;
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

void Flasher::GetEligibility(bool* to_upgrade_app,
                             bool* to_upgrade_bootloader) const {
  if (forceful_upgrade_) {
    *to_upgrade_app = true;
    *to_upgrade_bootloader = true;
    return;
  }

  // TODO(porce): When fw_hw_rev is a set, See if pl_hw_rev is a member of
  // fw_hw_rev.
  if (fw_hw_rev_ != pl_hw_rev_) {
    // Hardware revision mismatch. Do not upgrade.
    *to_upgrade_app = false;
    *to_upgrade_bootloader = false;
  }

  *to_upgrade_app = (fw_app_ver_ != pl_app_ver_);
  *to_upgrade_bootloader = (fw_bootloader_ver_ != pl_bootloader_ver_);
}

void Flasher::StoreFirmwareInfo() {
  std::string fw_app_ver, fw_bootloader_ver, fw_hw_rev;
  std::string err_msg;
  firmware_.ParseManifest(&fw_app_ver_, &fw_bootloader_ver_, &fw_hw_rev_);
}

void Flasher::StorePeripheralInfo() {
  // This special routine is a workaround:
  // The App version can be queries only in the App mode.
  MinicamDevice minidev_app(huddly::kVendorId, huddly::kProductIdApp);

  std::string err_msg;
  minidev_app.Setup(&err_msg);
  minidev_app.GetVersion(&pl_app_ver_, &pl_bootloader_ver_);
  minidev_app.GetHwRevision(&pl_hw_rev_, &err_msg);

  return;
}

}  // namespace huddly
