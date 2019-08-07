// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firmware.h"

#include <base/logging.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#ifdef __ANDROID__
#include <ftw.h>  // nftw
#endif

#include "manifest.h"
#include "tools.h"

namespace huddly {

namespace {
// Base directory to fetch binary and manifest files.
// kDefaultPkgDir from auto upgrade in ChromeOS, kManualPkgDir for manual
// upgrade.
#if __ANDROID__
// Core-Devices Android specific builds change for android alternatives.
const char kDefaultPkgDir[] = "/system/etc/peripheral_firmware/huddly/";
#else
const char kDefaultPkgDir[] = "/lib/firmware/huddly/";
#endif

#if __ANDROID__
// Consider acquiring tmp directory programatically or storing in memory.
const char kManualPkgDir[] = "/data/local/tmp/huddly/";
#else
const char kManualPkgDir[] = "/tmp/huddly/";
#endif
// Huddly firmware binary and manifest files, relative to the base directory.
const char kFirmwareApp[] = "bin/huddly.bin";
const char kFirmwareBootloader[] = "bin/huddly_boot.bin";
const char kFirmwareManifest[] = "manifest.json";
const char kDefaultVerStr[] = "unknown";
}  // namespace

Firmware::Firmware(const std::string& pkg_path) : pkg_path_(pkg_path) {
  if (!IsManualUpgrade()) {
    // ChromeOS auto upgrade workflow.
    pkg_dir_ = kDefaultPkgDir;
  } else {
    // Manual upgrade workflow.
    pkg_dir_ = kManualPkgDir;
  }

  if (!pkg_dir_.empty() && pkg_dir_.back() != '/') {
    pkg_dir_ += "/";
  }

  app_path_ = pkg_dir_ + kFirmwareApp;
  bootloader_path_ = pkg_dir_ + kFirmwareBootloader;
  manifest_path_ = pkg_dir_ + kFirmwareManifest;
}

Firmware::~Firmware() {
#if __ANDROID__
  if (IsManualUpgrade() && HasFile(pkg_dir_)) {
    nftw(pkg_dir_.c_str(), NftwDeleteFile, 10, FTW_DEPTH);
  }
#endif
}

bool Firmware::IsReady(std::string* err_msg) const {
  if (IsManualUpgrade()) {
    // Package file is given directly. Need to uncompress before proceeding.
    if (!HasFile(pkg_path_)) {
      *err_msg = ".. firmware package file does not exist: ";
      *err_msg += pkg_path_;
      return false;
    }

    if (!Uncompress(pkg_path_, err_msg)) {
      return false;
    }
  }

  if (!HasFile(app_path_)) {
    *err_msg = "firmware app not ready: " + std::string(app_path_);
    return false;
  }
  if (!HasFile(bootloader_path_)) {
    *err_msg =
        "firmware bootloader not ready: " + std::string(bootloader_path_);
    return false;
  }
  if (!IsManualUpgrade() && !HasFile(manifest_path_)) {
    // Manual upgrade workflow does not require the manifest.txt.
    *err_msg = "firmware manifest not ready: " + std::string(manifest_path_);
    return false;
  }

  return true;
}

bool Firmware::HasFile(const std::string& file_path) const {
  // Not only checking the precense, but also checks the
  // goodness in the sense of ifstream.
  std::ifstream f(file_path.c_str());
  return f.good();
}

bool Firmware::Uncompress(const std::string& file_path,
                          std::string* err_msg) const {
  if (!HasFile(pkg_dir_)) {
    LOG(INFO) << ".. Uncompressing " << file_path << " on " << pkg_dir_;
  } else {
    return true;
  }
#ifdef __ANDROID__
  return UncompressZip(pkg_dir_.c_str(), file_path.c_str(), err_msg);
#else
  // TODO(porce): Consider using standard library.
  std::string command = "mkdir -p " + pkg_dir_ + " && cd " + pkg_dir_ +
                        " && unzip -o " + pkg_path_;
  return RunCommand(command, err_msg);
#endif
}

bool Firmware::ParseManifestJSON(std::string* app_ver,
                                 std::string* bootloader_ver,
                                 std::string* hw_rev) const {
  const base::FilePath path(FILE_PATH_LITERAL(manifest_path_.c_str()));
  huddly::Manifest manifest(path);

  if (!manifest.ParseFile()) {
    // Fallback on defaults
    *app_ver = kDefaultVerStr;
    *bootloader_ver = kDefaultVerStr;
    *hw_rev = kDefaultVerStr;
    return false;
  }

  *app_ver = manifest.app_ver();
  *bootloader_ver = manifest.boot_ver();
  *hw_rev = manifest.hw_rev();
  return true;
}

void Firmware::ShowInfo() const {
  std::string app_ver, bootloader_ver, hw_rev;
  ParseManifestJSON(&app_ver, &bootloader_ver, &hw_rev);

  // Bypass the logging facility. libchrome logging does not honor stdout
  // properly. Instead it goes to stderr.
  std::cout << "Firmware package:" << std::endl;
  std::cout << "  dir:           " << pkg_dir_ << std::endl;
  std::cout << "  bootloader:    " << bootloader_ver << std::endl;
  std::cout << "  app:           " << app_ver << std::endl;
  std::cout << "  hw_rev:        " << hw_rev << std::endl;
}

}  // namespace huddly
