// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "hpk_updater.h"
#include "utils.h"

int main(int argc, char* argv[]) {
  const uint16_t kHuddlyUsbVendorId = 0x2bd9;

  DEFINE_string(log_to, "syslog", "Path to log file. stdout is legal.");
  DEFINE_bool(reboot, false, "Only reboot device");
  DEFINE_bool(read_error_log, false, "Only read error log");
  DEFINE_bool(erase_error_log, false, "Only erase error log");
  DEFINE_string(product_id, "0x0021", "USB product id (hex)");
  DEFINE_string(file_name, "/lib/firmware/huddly/huddly_iq.hpk",
                "Firmware image file");
  DEFINE_bool(force, false, "Force firmware upgrade");
  DEFINE_bool(udev_mode, false,
              "Use when triggered by udev. Will run only one update pass per "
              "invokation");
  DEFINE_string(configuration, "hmh",
                "Camera configuration to set after update");
  DEFINE_int32(v, 0, "Diagnostic message verbosity level");

  brillo::FlagHelper::Init(argc, argv, argv[0]);

  if (FLAGS_log_to == "syslog") {
    // Default to Syslog.
    brillo::InitLog(brillo::InitFlags::kLogToSyslog);
  } else if (FLAGS_log_to == "stdout") {
    // Stdout.
    logging::LoggingSettings logging_settings;
    logging::InitLogging(logging_settings);
  } else {
    // A particular log file is specified.
    logging::LoggingSettings logging_settings;
    logging_settings.logging_dest = logging::LOG_TO_FILE;
    logging_settings.log_file_path = FLAGS_log_to.c_str();
    logging_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
    logging::InitLogging(logging_settings);
  }

  LOG(INFO) << "Starting Huddly hpk updater";

  auto updater = huddly::HpkUpdater::Create(
      kHuddlyUsbVendorId, std::stoul(FLAGS_product_id, nullptr, 16));

  if (!updater) {
    LOG(ERROR) << "Failed to create HpkUpdater";
    return EXIT_FAILURE;
  }

  if (FLAGS_reboot) {
    LOG(INFO) << "Reboot only requested";
    return (updater->Reboot(true) ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  LOG(INFO) << "Reset cause: " << updater->GetResetCause();

  if (FLAGS_read_error_log) {
    LOG(INFO) << "Reading error log";
    std::string error_log;
    if (!updater->ReadErrorLog(&error_log)) {
      LOG(ERROR) << "Failed to read error log";
      return EXIT_FAILURE;
    }

    if (error_log.size() > 0) {
      LOG(INFO) << "Error log: ";
      LOG(INFO) << error_log;
    } else {
      LOG(INFO) << "Error log empty";
    }

    return EXIT_SUCCESS;
  }

  if (FLAGS_erase_error_log) {
    LOG(INFO) << "Erasing error log";
    if (!updater->EraseErrorLog()) {
      LOG(ERROR) << "Failed to erase error log";
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }

  if (!updater->DoUpdate(base::FilePath(FLAGS_file_name), FLAGS_force,
                         FLAGS_udev_mode, FLAGS_configuration)) {
    LOG(ERROR) << "Huddly hpk updater failed";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
