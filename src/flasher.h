// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_FLASHER_H_
#define SRC_FLASHER_H_

#include <string>

#include "firmware.h"
#include "minicam_device.h"

namespace huddly {

// class Flasher stands on top of a Firmware object and a MinicamDevice object.
// This flasher shall run in a single threaded process.
// It is illegal to delete the Firmware or Minicam object unless Flasher
// completes it job.
class Flasher {
 public:
  Flasher(const Firmware& firmware,
          MinicamDevice* minidev,
          bool forceful_upgrade,
          bool dry_run);
  ~Flasher();
  bool FlashAll(std::string* err_msg) const;
  bool FlashWithFile(const std::string& file_path, std::string* err_msg) const;
  void StoreFirmwareInfo();
  void StorePeripheralInfo();

 private:
  Flasher(const Flasher&) = delete;
  Flasher& operator=(const Flasher&) = delete;

  // Test if the peripheral needs to be upgraded.
  void GetEligibility(bool* to_upgrade_app, bool* to_upgrade_bootloader) const;

  // TODO(porce): make pointers / references protected during active flash op.
  const Firmware& firmware_;
  MinicamDevice* minidev_;

  // When true, skip eligibility check. Flash firmware always.
  bool forceful_upgrade_;

  // When true, do as usual except for commiting procedure. Skip it instead.
  bool dry_run_;

  std::string fw_app_ver_;
  std::string fw_bootloader_ver_;
  std::string fw_hw_rev_;
  std::string pl_app_ver_;
  std::string pl_bootloader_ver_;
  std::string pl_hw_rev_;
};

}  // namespace huddly

#endif  // SRC_FLASHER_H_
