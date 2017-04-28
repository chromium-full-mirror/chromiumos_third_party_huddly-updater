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

Flasher::Flasher(const Firmware& firmware, MinicamDevice* minidev)
    : firmware_(firmware), minidev_(minidev) {}

Flasher::~Flasher() {
  std::string err_msg;
  if (!minidev_->Teardown(&err_msg)) {
    err_msg += ".. failed to tear down the minidev Flasher used";
    std::cout << err_msg << std::endl;
  }
}

bool Flasher::FlashApp(std::string* err_msg) const {
  if (!minidev_->Setup(err_msg)) {
    *err_msg += ".. failed to flash app in minidev setting up";
    return false;
  }

  if (!IsEligible(err_msg)) {
    *err_msg += ".. upgrade not eligible";
    return false;
  }

  uint32_t file_size;
  if (!GetFileSize(firmware_.app_path(), &file_size, err_msg)) {
    *err_msg += ".. failed to flash app. unknown file size";
    return false;
  }

  std::unique_ptr<uint8_t[]> data(new uint8_t[file_size]);

  uint32_t data_len =
      ReadFileToArray(firmware_.app_path(), file_size, data.get(), err_msg);

  if (data_len != file_size) {
    char msg[100];
    snprintf(msg, sizeof(msg),
             ".. failed to load the file contents to memory.(%d/%d) loaded. "
             "cancel flashing app. ",
             data_len, file_size);
    *err_msg += msg;
    return false;
  }

  if (!minidev_->WriteImage(data_len, data.get(), err_msg)) {
    *err_msg += ".. failed to flash app.";
    return false;
  }

  if (!minidev_->Teardown(err_msg)) {
    *err_msg += ".. failed to tear down the minidev Flasher used";
    return false;
  }
  return true;
}

bool Flasher::IsEligible(std::string* err_msg) const {
  uint8_t hw_rev;
  if (!minidev_->GetHwRevision(&hw_rev, err_msg)) {
    *err_msg += ". failed to get hardware revision";
    return false;
  }
  // TODO(porce): Test should be done against the manifest.json.
  const uint8_t kSupportedHwRev = 6;
  if (hw_rev != kSupportedHwRev) {
    *err_msg += ".. unsupported hardware revision: " + std::to_string(hw_rev);
    return false;
  }

  // TODO(porce): Check the firmware version match.

  return true;
}

}  // namespace huddly
