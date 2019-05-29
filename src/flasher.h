// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_FLASHER_H_
#define SRC_FLASHER_H_

#include <memory>
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
   Flasher(const Firmware &firmware, bool forceful_upgrade, bool dry_run,
           std::string serial_number = "");
   ~Flasher();

   // Returns true if firmware package is ready, and if it succeeds to query
   // / store the firmware versions of firmware package and the peripheral, and
   // to determine the upgrade eligibility. Returns false otherwise.
   bool Init(std::string *err_msg, const std::string &usb_path = "");

   bool IsEligibleForUpgrade() const {
     return is_eligible_to_upgrade_app_ || is_eligible_to_upgrade_boot_;
  }

  // Prints if bootloader and app need upgrades, together with current versions
  // and would-be versions. If any of them is eligible to upgrade, say the
  // device is eligible to upgrade.
  void ShowUpgradeEligibility() const;
  std::string GetUpgradeEligibilityMessage() const;

  bool FlashAll(std::string* err_msg) const;
  bool FlashWithFile(const std::string& file_path, std::string* err_msg) const;

 private:
  Flasher(const Flasher&) = delete;
  Flasher& operator=(const Flasher&) = delete;

  // Updates fw_app_ver_, fw_bootloader_ver_, fw_hw_rev_ with information
  // from file manifest.txt.
  void StoreFirmwareInfo();

  // Updates pl_app_ver_, pl_bootloader_ver_, pl_hw_rev_ with information
  // from the attached Huddly camera device.
  bool StorePeripheralInfo(std::string *err_msg,
                           const std::string &usb_path = "");

  // TODO(porce): make pointers / references protected during active flash op.
  const Firmware& firmware_;
  std::unique_ptr<MinicamDevice> minidev_;

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

  // Set by Init(), consumed by IsEligibleForUpgrade().
  bool is_eligible_to_upgrade_app_;
  bool is_eligible_to_upgrade_boot_;
};

}  // namespace huddly

#endif  // SRC_FLASHER_H_
