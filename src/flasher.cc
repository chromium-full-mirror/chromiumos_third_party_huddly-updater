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
Flasher::~Flasher() {}

bool Flasher::FlashApp(std::string* err_msg) const {
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

  return true;
}

}  // namespace huddly
