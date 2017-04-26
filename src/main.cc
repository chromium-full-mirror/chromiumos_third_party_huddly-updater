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

bool WaitForUsbDevice(uint16_t vendor_id, uint16_t product_id);
bool IsCameraAttached(int16_t product_id, std::string* err_msg);
bool HasCamera();
bool EnterBootloaderMode();
bool HasCameraBootloader(huddly::MinicamDevice* minidev);

int main(void) {
  std::cout << "Starting Huddly Package Updater .." << std::endl;

  if (!HasCamera()) {  // printing errors inside, upon failure.
    return EXIT_FAILURE;
  }

  // Has camera in APP state.
  if (!EnterBootloaderMode()) {  // printing errors inside, upon failure.
    return EXIT_FAILURE;
  }

  // Detected a camera in Bootloader mode
  huddly::MinicamDevice minidev(huddly::kProductIdBootloader);

  if (!HasCameraBootloader(&minidev)) {
    // printing errors inside, upon failure.
    return EXIT_FAILURE;
  }

  // Prepare the firmware package/images
  huddly::Firmware firmware;
  std::string err_msg;
  if (!firmware.IsReady(&err_msg)) {
    std::cout << ".. Firmware is not ready: " << err_msg << std::endl;
    return EXIT_FAILURE;
  }

  // Flash App
  huddly::Flasher flasher(firmware, &minidev);

  if (!flasher.FlashApp(&err_msg)) {
    err_msg += ".. failed to flash";
    std::cout << err_msg << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << ".. rebooting back to APP mode" << std::endl;
  if (!minidev.RebootInMode(huddly::BootMode::APP, &err_msg)) {
    std::cout << ".. failed to reboot back to APP mode" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << ".. upgrade complete." << std::endl;
  return EXIT_SUCCESS;
}

bool IsCameraAttached(int16_t product_id, std::string* err_msg) {
  huddly::UsbDevice usbdev(huddly::kVendorId, product_id);
  if (!usbdev.Setup(err_msg)) {
    *err_msg += ".. not found: " + std::to_string(product_id);
    return false;
  }

  huddly::MinicamDevice minidev(product_id);
  if (!minidev.Setup(err_msg)) {
    *err_msg +=
        ".. failed to Setup as Minicamdevice " + std::to_string(product_id);
    return false;
  }

  uint8_t hw_rev;
  if (!minidev.GetHwRevision(&hw_rev, err_msg)) {
    *err_msg += ". failed to get hardware revision";
    return false;
  }
  if (hw_rev != 6) {
    *err_msg += ".. unsupported hardware revision: " + std::to_string(hw_rev) +
                " in product id: " + std::to_string(product_id);
    return false;
  }
  return true;
}

bool WaitForUsbDevice(uint16_t vendor_id, uint16_t product_id) {
  const int kRetries = 30;

  std::string err_msg;
  huddly::UsbDevice usbdev(vendor_id, product_id);
  for (int retry = 0; retry < kRetries; retry++) {
    if (usbdev.Setup(&err_msg)) {
      // Device came up;
      return true;
    }

    huddly::SleepMilliSec(1000);  // 1 sec
    continue;
  }
  return false;
}

bool HasCamera() {
  std::string err_msg;

  if (IsCameraAttached(huddly::kProductIdBootloader, &err_msg)) {
    // Abnormal initial state. Upgrading from this initial state is not
    // supported. Reboot in APP mode to bring it back to normal state. For users
    // performing upgrade manually: rerun this program.

    std::cout << ".. detected abnormal initial state: Bootloader" << std::endl;
    std::cout << ".. rebooting in APP mode" << std::endl;
    huddly::MinicamDevice minidev(huddly::kProductIdBootloader);
    if (!minidev.Setup(&err_msg)) {
      err_msg += ".. failed to setup for rebooting.";
      std::cout << err_msg << std::endl;
      return false;
    }

    std::cout << ".. successfully rebooting. Rerun this upgrader manually."
              << std::endl;
    return false;
  }

  // Camera is not in Bootloader mode
  if (!IsCameraAttached(huddly::kProductIdApp, &err_msg)) {
    err_msg += ".. no camera is attached.";
    std::cout << err_msg << std::endl;
    return false;
  }

  return true;
}

bool EnterBootloaderMode() {
  std::string err_msg;
  huddly::MinicamDevice minidev_app(huddly::kProductIdApp);
  if (minidev_app.Setup(&err_msg) &&
      minidev_app.RebootInMode(huddly::BootMode::BOOTLOADER, &err_msg) &&
      WaitForUsbDevice(huddly::kVendorId, huddly::kProductIdBootloader)) {
    std::cout << ".. rebooted in BOOTLOADER mode" << err_msg << std::endl;
    return true;
  }

  err_msg += ".. failed to upgrade";
  std::cout << err_msg << std::endl;
  return false;
}

bool HasCameraBootloader(huddly::MinicamDevice* minidev) {
  std::string err_msg;

  if (minidev->Setup(&err_msg)) {
    return true;
  }

  err_msg += ".. failed to upgrade. attempt to recover to APP mode";
  std::cout << err_msg << std::endl;

  // Escape sequence: Already in strange state. Best effort recovery.
  if (!minidev->RebootInMode(huddly::BootMode::APP, &err_msg)) {
    err_msg +=
        ".. failed to recover from unexpected state. The device might be in "
        "BOOTLOADER mode.";
    std::cout << err_msg << std::endl;
    return false;
  }

  std::cout << ".. rebooting in APP mode" << std::endl;
  return false;
}
