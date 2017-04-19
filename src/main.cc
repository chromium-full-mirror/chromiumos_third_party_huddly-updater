// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <iostream>

#include "firmware.h"

int main(void) {
  std::cout << "Starting Huddly Package Updater .." << std::endl;

  huddly::Firmware firmware;

  std::string err_msg;
  if (!firmware.IsReady(&err_msg)) {
    std::cout << ".. Firmware is not ready: " << err_msg << std::endl;
    return EXIT_FAILURE;
  }

  // TODO(porce): Write UsbDevice class, initialized by VID and PID.
  // psdudo code plan
  // UsbDevice usbdev(VID, PID);
  // if (!usbdev.IsUpgradeEligible(firmware)) { byebye; }
  // usbdev.FlashFirmware();
  // usbdev.VerifyFirmware();

  return EXIT_SUCCESS;
}
