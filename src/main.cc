// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <getopt.h>

#include <cstdlib>
#include <iostream>
#include <string>

#include "firmware.h"
#include "flasher.h"
#include "minicam_device.h"
#include "tools.h"
#include "usb_device.h"

namespace {

const int kWaitRebootingTimeoutSec = 10;

}  // namespace

struct CmdlineFlags {
  bool show_info;
  bool forceful_upgrade;
  bool dry_run;
  std::string pkg_path;  // Absolute path for huddly.pkg file.
};

// Switch booting modes and reboot.
bool SwitchMode(huddly::BootMode from,
                huddly::BootMode to,
                std::string* err_msg);

// Show firmware versions and hardware revisions of firmware package and
// the Huddly peripheral in use.
void ShowInfo(const CmdlineFlags& cmd_flags);

// Recover from, if any, the Bootloader mode to App mode.
void CheckAndRescue();

// Show command line argument options.
void ShowUsage(const char* program_name);

// Parse command line arguments.
void ParseArgs(int argc, char* argv[], CmdlineFlags* flags);

int main(int argc, char* argv[]) {
  // If the camera is in abnormal state, attempt to rescue it first.
  CheckAndRescue();

  // Hanlde command line options.
  CmdlineFlags cmd_flags = {};
  ParseArgs(argc, argv, &cmd_flags);
  if (cmd_flags.show_info) {
    ShowInfo(cmd_flags);
    return EXIT_SUCCESS;
  }

  std::cout << "Starting Huddly Package Updater .." << std::endl;

  huddly::Firmware firmware(cmd_flags.pkg_path);
  huddly::Flasher flasher(firmware, cmd_flags.forceful_upgrade,
                          cmd_flags.dry_run);

  std::string err_msg;
  if (!flasher.Init(&err_msg)) {
    std::cout << err_msg << std::endl;
  }
  if (!flasher.IsEligibleForUpgrade()) {
    // Not necessarily an error, but propagates useful info.
    std::cout << ".. is not eligible for firmware upgrade" << std::endl;
    return EXIT_SUCCESS;
  }

  if (!SwitchMode(huddly::BootMode::APP, huddly::BootMode::BOOTLOADER,
                  &err_msg)) {
    err_msg += ".. failed to initialize the mode";
    std::cout << err_msg << std::endl;
    return EXIT_FAILURE;
  }

  // Flash App.
  if (!flasher.FlashAll(&err_msg)) {
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

void CheckAndRescue() {
  // TODO(porce): Implement automated recovery. Consider not to bail out.

  // Object of huddly camera in Bootloader mode.
  huddly::MinicamDevice minidev_boot(huddly::kVendorId,
                                     huddly::kProductIdBootloader);

  std::string err_msg;
  // This conditional is to support to recover from very unusual case.
  if (!minidev_boot.CheckIfExists()) {
    return;
  }

  // Abnormal situation. Reboot in App mode and exit.
  if (!SwitchMode(huddly::BootMode::BOOTLOADER, huddly::BootMode::APP,
                  &err_msg)) {
    err_msg += ".. failed to initialize the mode";
    std::cout << err_msg << std::endl;
  }
  std::exit(EXIT_FAILURE);
}

void ShowInfo(const CmdlineFlags& cmd_flags) {
  huddly::Firmware firmware(cmd_flags.pkg_path);
  firmware.ShowInfo();

  huddly::MinicamDevice minidev(huddly::kVendorId, huddly::kProductIdApp);
  std::string err_msg;
  if (!minidev.Setup(&err_msg)) {
    std::cout << err_msg << std::endl;
    return;
  }
  minidev.ShowInfo();
}

void ShowUsage(const char* program_name) {
  // TODO(porce): Refactor using const string.
  /* clang-format off */
  printf("\n"
    "Usage: %s [--option(s)]\n"
    "  --help:    Show this usage.\n"
    "  --info:    Show firmware and peripheral information.\n"
    "  --force:   Force the upgrade ignoring the upgrade eligibility check.\n"
    "  --dryrun:  Dryrun. Do everything except for final committment.\n"
    "  --pkg:     Specify the absolute path for firmware package file.\n"
    "\n",
    program_name);
  /* clang-format on */
}

// TODO(crbug.com/719567): Replace with base::CommandLine.
void ParseArgs(int argc, char* argv[], CmdlineFlags* flags) {
  const char* kOptString = "h";
  const struct option long_options[] = {
      {"help", no_argument, 0, 'h'},      {"info", no_argument, 0, 'i'},
      {"force", no_argument, 0, 'f'},     {"dryrun", no_argument, 0, 'd'},
      {"pkg", required_argument, 0, 'p'}, {0, 0, 0, 0},
  };

  flags->pkg_path = "";

  int opt;
  int longoption_index = 0;
  while ((opt = getopt_long(argc, argv, kOptString, long_options,
                            &longoption_index)) != -1) {
    switch (opt) {
      case 'h':
        ShowUsage(argv[0]);
        std::exit(EXIT_SUCCESS);
      case 'i':
        flags->show_info = true;
        break;
      case 'f':
        flags->forceful_upgrade = true;
        break;
      case 'd':
        flags->dry_run = true;
        break;
      case 'p':
        flags->pkg_path = std::string(optarg);
        break;
      default:
        ShowUsage(argv[0]);
        std::exit(EXIT_FAILURE);
        break;
    }
  }
}
