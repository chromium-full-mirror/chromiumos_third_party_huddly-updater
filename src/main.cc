// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <iostream>

#include "firmware.h"
#include "flasher.h"
#include "minicam_device.h"
#include "tools.h"
#include "usb_device.h"

const int kWaitRebootingTimeoutSec = 10;

// Switch booting modes and reboot.
bool SwitchMode(huddly::BootMode from,
                huddly::BootMode to,
                std::string* err_msg);

int main(void) {
  std::cout << "Starting Huddly Package Updater .." << std::endl;

  // Object of huddly camera in Bootloader mode.
  huddly::MinicamDevice minidev_boot(huddly::kVendorId,
                                     huddly::kProductIdBootloader);

  std::string err_msg;
  // This conditional is to support to recover from very unusual case.
  if (minidev_boot.CheckIfExists()) {
    // Abnormal situation. Reboot in App mode and exit.
    if (!SwitchMode(huddly::BootMode::BOOTLOADER, huddly::BootMode::APP,
                    &err_msg)) {
      err_msg += ".. failed to initialize the mode";
      std::cout << err_msg << std::endl;
    }
    return EXIT_FAILURE;
  }

  // Object of huddly camera in App mode.
  huddly::MinicamDevice minidev_app(huddly::kVendorId, huddly::kProductIdApp);
  if (!minidev_app.CheckIfExists()) {
    err_msg += ".. camera not detected";
    std::cout << err_msg << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << ".. camera detected: " << minidev_app.GetId() << std::endl;
  // Camera is detected.

  if (!SwitchMode(huddly::BootMode::APP, huddly::BootMode::BOOTLOADER,
                  &err_msg)) {
    err_msg += ".. failed to initialize the mode";
    std::cout << err_msg << std::endl;
    return EXIT_FAILURE;
  }

  // Prepare the firmware package/images.
  huddly::Firmware firmware;
  if (!firmware.IsReady(&err_msg)) {
    err_msg += ".. Firmware is not ready";
    std::cout << err_msg << std::endl;
    return EXIT_FAILURE;
  }

  // Flash App.
  huddly::Flasher flasher(firmware, &minidev_boot);
  if (!flasher.FlashApp(&err_msg)) {
    err_msg += ".. failed to flash";
    std::cout << err_msg << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << ".. wrote firmware" << std::endl;

  if (!SwitchMode(huddly::BootMode::BOOTLOADER, huddly::BootMode::APP,
                  &err_msg)) {
    err_msg += ".. failed to reboot back to APP mode";
    std::cout << err_msg << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << ".. upgrade complete." << std::endl;
  return EXIT_SUCCESS;
}

bool SwitchMode(huddly::BootMode from,
                huddly::BootMode to,
                std::string* err_msg) {
  huddly::MinicamDevice minidev_from(huddly::kVendorId,
                                     BootModeToProductId(from));

  if (!minidev_from.Setup(err_msg)) {
    *err_msg += ".. failed to USB access setup";
    return false;
  }

  std::cout << ".. switching mode from " << BootModeStr(from) << " to "
            << BootModeStr(to) << ".. rebooting (timeout "
            << kWaitRebootingTimeoutSec << " sec)" << std::endl;

  if (!minidev_from.RebootInMode(to, err_msg)) {
    *err_msg += ".. failed to reboot in mode: ";
    *err_msg += BootModeStr(to);
    return false;
  }

  std::cout << ".. waiting to come up" << std::endl;

  huddly::MinicamDevice minidev_to(huddly::kVendorId, BootModeToProductId(to));
  if (!minidev_to.WaitForOnline(kWaitRebootingTimeoutSec, err_msg)) {
    *err_msg += ".. waited but ";
    *err_msg += BootModeStr(to) + " mode did not come up";
    return false;
  }

  std::cout << ".. entered " << BootModeStr(to)
            << " mode. VID:PID=" << minidev_to.GetId() << std::endl;
  return true;
}
