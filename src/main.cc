// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <getopt.h>

#include <cstdlib>
#include <string>

#include "firmware.h"
#include "flasher.h"
#include "minicam_device.h"
#include "tools.h"
#include "usb_device.h"

namespace {

const int kWaitRebootingTimeoutSec = 10;

// TODO(porce): Investigate the permission of /var/log/huddly.log
const char kDefaultLogFile[] = "/tmp/huddly.log";

}  // namespace

struct CmdlineFlags {
  bool show_info;
  bool forceful_upgrade;
  bool dry_run;
  std::string pkg_path;  // Absolute path for huddly.pkg file.
  std::string log_to;    // Path of log file destination. stdout is legal.
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
  // Hanlde command line options.
  CmdlineFlags cmd_flags = {};
  ParseArgs(argc, argv, &cmd_flags);

  if (cmd_flags.log_to != "stdout") {
    logging::LoggingSettings logging_settings;
    logging_settings.logging_dest = logging::LOG_TO_FILE;
    logging_settings.log_file = cmd_flags.log_to.c_str();
    logging_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
    logging::InitLogging(logging_settings);
  }

  LOG(INFO) << "Starting Huddly Package Updater ..";

  // If the camera is in abnormal state, attempt to rescue it first.
  CheckAndRescue();

  if (cmd_flags.show_info) {
    LOG(INFO) << "Show info..";
    ShowInfo(cmd_flags);
    return EXIT_SUCCESS;
  }

  huddly::Firmware firmware(cmd_flags.pkg_path);
  huddly::Flasher flasher(firmware, cmd_flags.forceful_upgrade,
                          cmd_flags.dry_run);

  std::string err_msg;
  if (!flasher.Init(&err_msg)) {
    LOG(ERROR) << err_msg;
  }
  if (!flasher.IsEligibleForUpgrade()) {
    // Not necessarily an error, but propagates useful info.
    LOG(INFO) << ".. is not eligible for firmware upgrade";
    return EXIT_SUCCESS;
  }

  if (!SwitchMode(huddly::BootMode::APP, huddly::BootMode::BOOTLOADER,
                  &err_msg)) {
    err_msg += ".. failed to initialize the mode";
    LOG(ERROR) << err_msg;
    return EXIT_FAILURE;
  }

  // Flash App.
  if (!flasher.FlashAll(&err_msg)) {
    err_msg += ".. failed to flash";
    LOG(ERROR) << err_msg;
    return EXIT_FAILURE;
  }

  LOG(INFO) << ".. wrote firmware";

  if (!SwitchMode(huddly::BootMode::BOOTLOADER, huddly::BootMode::APP,
                  &err_msg)) {
    err_msg += ".. failed to reboot back to APP mode";
    LOG(ERROR) << err_msg;
    return EXIT_FAILURE;
  }

  LOG(INFO) << ".. upgrade complete.";
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

  LOG(INFO) << ".. switching mode from " << BootModeStr(from) << " to "
            << BootModeStr(to) << ".. rebooting (timeout "
            << kWaitRebootingTimeoutSec << " sec)";

  if (!minidev_from.RebootInMode(to, err_msg)) {
    *err_msg += ".. failed to reboot in mode: ";
    *err_msg += BootModeStr(to);
    return false;
  }

  LOG(INFO) << ".. waiting to come up";

  huddly::MinicamDevice minidev_to(huddly::kVendorId, BootModeToProductId(to));
  if (!minidev_to.WaitForOnline(kWaitRebootingTimeoutSec, err_msg)) {
    *err_msg += ".. waited but ";
    *err_msg += BootModeStr(to) + " mode did not come up";
    return false;
  }

  LOG(INFO) << ".. entered " << BootModeStr(to)
            << " mode. VID:PID=" << minidev_to.GetId();
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

  LOG(INFO) << "Bootloader mode detected. Switching to App mode";
  // Abnormal situation. Reboot in App mode and exit.
  if (!SwitchMode(huddly::BootMode::BOOTLOADER, huddly::BootMode::APP,
                  &err_msg)) {
    err_msg += ".. failed to initialize the mode";
    LOG(INFO) << err_msg;
  }
  std::exit(EXIT_FAILURE);
}

void ShowInfo(const CmdlineFlags& cmd_flags) {
  huddly::Firmware firmware(cmd_flags.pkg_path);
  firmware.ShowInfo();

  huddly::MinicamDevice minidev(huddly::kVendorId, huddly::kProductIdApp);
  std::string err_msg;
  if (!minidev.Setup(&err_msg)) {
    LOG(ERROR) << err_msg;
    return;
  }
  minidev.ShowInfo();
}

void ShowUsage(const char* program_name) {
  // TODO(porce): Refactor using const string.
  LOG(INFO) << "Show usage";
  /* clang-format off */
  printf("\n"
    "Usage: %s [--option(s)]\n"
    "  --help:    Show this usage.\n"
    "  --info:    Show firmware and peripheral information.\n"
    "  --force:   Force the upgrade ignoring the upgrade eligibility check.\n"
    "  --dryrun:  Dryrun. Do everything except for final committment.\n"
    "  --pkg:     Specify the absolute path for firmware package file.\n"
    "  --log_to:  Path to log file. stdout is legal. \n"
    "\n",
    program_name);
  /* clang-format on */
}

// TODO(crbug.com/719567): Replace with base::CommandLine.
void ParseArgs(int argc, char* argv[], CmdlineFlags* flags) {
  const char* kOptString = "h";
  const struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"info", no_argument, 0, 'i'},
      {"force", no_argument, 0, 'f'},
      {"dryrun", no_argument, 0, 'd'},
      {"pkg", required_argument, 0, 'p'},
      {"log_to", required_argument, 0, 'l'},
      {0, 0, 0, 0},
  };

  flags->pkg_path = "";
  flags->log_to = kDefaultLogFile;

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
      case 'l':
        flags->log_to = std::string(optarg);
        break;
      default:
        ShowUsage(argv[0]);
        std::exit(EXIT_FAILURE);
        break;
    }
  }
}
