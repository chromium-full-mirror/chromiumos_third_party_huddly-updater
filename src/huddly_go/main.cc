// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <brillo/syslog_logging.h>
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

}  // namespace

struct CmdlineFlags {
  bool show_info;
  bool forceful_upgrade;
  bool dry_run;
  std::string pkg_path;  // Absolute path for huddly.pkg file.
  std::string log_to;    // Path of log file destination. stdout is legal.
  bool prep_only;
  bool burn_only;
  bool switch_to_app_mode;
  bool switch_to_boot_mode;
};

// Switch booting modes and reboot.
bool SwitchMode(huddly::BootMode from,
                huddly::BootMode to,
                std::string* err_msg);

// Show firmware versions and hardware revisions of firmware package and
// the Huddly peripheral in use.
void ShowInfo(const CmdlineFlags& cmd_flags);

// Sets additional configuration properties for the Huddly Go Camera.
bool ConfigCamera();

// Show command line argument options.
void ShowUsage(const char* program_name);

// Parse command line arguments.
void ParseArgs(int argc, char* argv[], CmdlineFlags* flags);

int main(int argc, char* argv[]) {
  // Hanlde command line options.
  CmdlineFlags cmd_flags = {};
  ParseArgs(argc, argv, &cmd_flags);

  // In the simplified workflow, do both do_prep and do_burn steps.
  // In CFM workflow, two steps are separated by two different udev rules
  // to accommodate the minijail restrictions.
  // TODO(porce): Check the exclusiveness of |burn_only| and |prep_only| and
  // warn.
  bool do_prep = !cmd_flags.burn_only;
  bool do_burn = !cmd_flags.prep_only;

  if (cmd_flags.burn_only) {
    cmd_flags.forceful_upgrade = true;  // Override
  }

  if (cmd_flags.log_to.empty()) {
    // Default to Syslog.
    brillo::InitLog(brillo::InitFlags::kLogToSyslog);
  } else if (cmd_flags.log_to == "stdout") {
    // Stdout.
    logging::LoggingSettings logging_settings;
    logging::InitLogging(logging_settings);
  } else {
    // A particular log file is specified.
    logging::LoggingSettings logging_settings;
    logging_settings.logging_dest = logging::LOG_TO_FILE;
    logging_settings.log_file_path = cmd_flags.log_to.c_str();
    logging_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
    logging::InitLogging(logging_settings);
  }

  LOG(INFO) << "Starting Huddly Package Updater ..";

  if (cmd_flags.show_info) {
    LOG(INFO) << "Show info..";
    ShowInfo(cmd_flags);
    return EXIT_SUCCESS;
  }

  std::string err_msg;
  // TODO(porce): Check the exclusiveness of |switch_to_app_mode| and
  // |switch_to_boot_mode| and warn.

  // TODO(porce): Factor out into helper functions for return cases below.
  if (cmd_flags.switch_to_app_mode) {
    if (!SwitchMode(huddly::BootMode::BOOTLOADER, huddly::BootMode::APP,
                    &err_msg)) {
      err_msg += ".. failed to initialize the mode";
      LOG(INFO) << err_msg;
      return EXIT_FAILURE;
    }
    LOG(INFO) << ".. succeeded in switching mode to app";
    return EXIT_SUCCESS;
  }
  if (cmd_flags.switch_to_boot_mode) {
    if (!SwitchMode(huddly::BootMode::APP, huddly::BootMode::BOOTLOADER,
                    &err_msg)) {
      err_msg += ".. failed to initialize the mode";
      LOG(INFO) << err_msg;
      return EXIT_FAILURE;
    }
    LOG(INFO) << ".. succeeded in switching mode to boot";
    return EXIT_SUCCESS;
  }

  huddly::Firmware firmware(cmd_flags.pkg_path);
  huddly::Flasher flasher(firmware, cmd_flags.forceful_upgrade,
                          cmd_flags.dry_run);

  if (!flasher.Init(&err_msg)) {
    LOG(ERROR) << err_msg;
  }

  if (do_prep) {
    LOG(INFO) << flasher.GetUpgradeEligibilityMessage();
    if (!flasher.IsEligibleForUpgrade()) {

      // If configuring camera properties fails, or is not necessary, display
      // message and quit.
      if (!ConfigCamera())
        LOG(INFO) << ".. Exiting huddly-updater.";

      return EXIT_SUCCESS;
    }

    if (!SwitchMode(huddly::BootMode::APP, huddly::BootMode::BOOTLOADER,
                    &err_msg)) {
      err_msg += ".. failed to initialize the mode";
      LOG(ERROR) << err_msg;
      return EXIT_FAILURE;
    }
  }

  if (do_burn) {
    if (!flasher.FlashAll(&err_msg)) {
      err_msg += ".. failed to flash";
      LOG(ERROR) << err_msg;

      err_msg = "";
      if (!SwitchMode(huddly::BootMode::BOOTLOADER, huddly::BootMode::APP,
                      &err_msg)) {
        err_msg += ".. failed to reboot back to APP mode";
        LOG(ERROR) << err_msg;
      }

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
  }

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

// Configures the number of streams sent from Huddly camera.
bool ConfigStreamMode(const huddly::MinicamDevice &minidev) {
  const huddly::StreamMode kMode = huddly::StreamMode::DUAL;
  std::string err_msg;

  huddly::StreamMode current_mode;
  if (!minidev.GetStreamMode(&current_mode)) {
    LOG(ERROR) << "Failed to get stream mode.";
    return false;
  }

  if (current_mode == kMode) {
    LOG(INFO) << ".. Already in " << huddly::StreamModeToStr(kMode) << " mode.";
    return false;
  }

  if (!minidev.SetStreamMode(kMode, &err_msg)) {
    LOG(ERROR) << "Failed to set stream mode: " << err_msg;
    return false;
  }

  LOG(INFO) << "Changed stream mode from "
            << huddly::StreamModeToStr(current_mode) << " to "
            << huddly::StreamModeToStr(kMode) << ". Reboot needed.";
  return true;
}

// Configures the property to force the Huddly Go Camera into High Speed Mode.
bool ConfigHighSpeedMode(const huddly::MinicamDevice &minidev) {
  const bool kForceHighSpeedMode = false;
  std::string err_msg;

  bool force_high_speed;
  if (!minidev.GetForceHighSpeedMode(&force_high_speed, &err_msg)) {
    LOG(ERROR) << "Failed to query property: force-hs-only. " << err_msg;
    return false;
  }

  if (force_high_speed == kForceHighSpeedMode) {
    LOG(INFO) << ".. Already in "
              << (force_high_speed ? "HighSpeed" : "Default") << " USB mode.";
    return false;
  }

  if (!minidev.SetForceHighSpeedMode(kForceHighSpeedMode, &err_msg)) {
    LOG(ERROR) << "Failed to set property: force-hs-only. " << err_msg;
    return false;
  }

  LOG(INFO) << "Changed usb mode from "
            << (force_high_speed ? "HighSpeed" : "Default") << " to "
            << (kForceHighSpeedMode ? "HighSpeed" : "Default")
            << ". Reboot needed";
  return true;
}

bool ConfigCamera() {
  huddly::MinicamDevice minidev(huddly::kVendorId, huddly::kProductIdApp);
  std::string err_msg;
  if (!minidev.Setup(&err_msg)) {
    LOG(ERROR) << err_msg;
    return false;
  }

  bool stream_reboot = ConfigStreamMode(minidev);
  bool usb_reboot = ConfigHighSpeedMode(minidev);

  if (stream_reboot || usb_reboot) {
    LOG(INFO) << ".. Rebooting device.";
    minidev.Reboot(&err_msg);
    return true;
  }

  minidev.Teardown(&err_msg);
  return false;
}

void ShowUsage(const char* program_name) {
  LOG(INFO) << "Show usage";
  /* clang-format off */
  printf("\n"
    "Usage: %s [--option(s)]\n"
    "  --app:     Switch to App mode and reboot.\n"
    "  --boot:    Switch to Boot mode and reboot.\n"
    "  --burn:    Burn only, without prepping step.\n"
    "  --dryrun:  Dryrun. Do everything except for final committment.\n"
    "  --force:   Force the upgrade ignoring the upgrade eligibility check.\n"
    "  --help:    Show this message and quit.\n"
    "  --info:    Show firmware and peripheral information.\n"
    "  --log_to:  Path to log file. stdout is legal.\n"
    "  --pkg:     Specify the absolute path for firmware package file.\n"
    "  --prep:    Prep only, without entering the burning step.\n"
    "\n",
    program_name);
  /* clang-format on */
}

// TODO(crbug.com/719567): Replace with base::CommandLine.
void ParseArgs(int argc, char* argv[], CmdlineFlags* flags) {
  const char* kOptString = "h";
  const struct option long_options[] = {
      {"app",    no_argument,       0, 'a'},
      {"boot",   no_argument,       0, 'b'},
      {"burn",   no_argument,       0, 'u'},
      {"dryrun", no_argument,       0, 'd'},
      {"force",  no_argument,       0, 'f'},
      {"help",   no_argument,       0, 'h'},
      {"info",   no_argument,       0, 'i'},
      {"log_to", required_argument, 0, 'l'},
      {"pkg",    required_argument, 0, 'p'},
      {"prep",   no_argument,       0, 'r'},
      {0, 0, 0, 0},
  };

  flags->pkg_path = "";
  flags->log_to = "";
  flags->prep_only = false;
  flags->burn_only = false;
  flags->switch_to_app_mode = false;
  flags->switch_to_boot_mode = false;

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
      case 'r':
        flags->prep_only = true;
        break;
      case 'u':
        flags->burn_only = true;
        break;
      case 'a':
        flags->switch_to_app_mode = true;
        break;
      case 'b':
        flags->switch_to_boot_mode = true;
        break;
      default:
        ShowUsage(argv[0]);
        std::exit(EXIT_FAILURE);
        break;
    }
  }
}
