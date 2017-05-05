// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "firmware.h"
#include "tools.h"

namespace huddly {

namespace {
// Huddly firmware package file and its uncompressed files.
// These are relative path to the directory specified by the
// command line argument. Default: kDefaultPkgDir
const char kFirmwareApp[] = "bin/huddly.bin";
const char kFirmwareBootloader[] = "bin/huddly_boot.bin";
const char kFirmwareManifest[] = "manifest.txt";
const char kFirmwarePkg[] = "huddly.pkg";

bool ParseManifestLine(const std::string& line,
                       std::string* key,
                       std::string* value) {
  const char* kDelimiter = ":";
  // TODO(crbug.com/719567): Use libchrome for string splitting.
  size_t pos = line.find(kDelimiter);
  if (pos == std::string::npos || pos == 0) {
    return false;
  }
  *key = line.substr(0, pos);
  *value = line.substr(pos + 1);
  return true;
}

}  // namespace

Firmware::Firmware(const std::string& pkg_dir) : pkg_dir_(pkg_dir) {
  if (!pkg_dir_.empty() && pkg_dir_.back() != '/') {
    pkg_dir_ += "/";
  }

  pkg_path_ = pkg_dir_ + kFirmwarePkg;
  app_path_ = pkg_dir_ + kFirmwareApp;
  bootloader_path_ = pkg_dir_ + kFirmwareBootloader;
  manifest_path_ = pkg_dir_ + kFirmwareManifest;
}

Firmware::~Firmware() {}

bool Firmware::IsReady(std::string* err_msg) const {
  if (!HasFile(pkg_path_)) {
    *err_msg = "firmware pkg not ready: " + std::string(pkg_path_);
    return false;
  }
  if (!Uncompress(pkg_path_, err_msg)) {
    return false;
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
  if (!HasFile(manifest_path_)) {
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
  std::cout << ".. Uncompressing " << file_path << " on " << pkg_dir_
            << std::endl;
  // TODO(porce): Consider using standard library.
  std::string command = "mkdir -p " + pkg_dir_ + " && cd " + pkg_dir_ +
                        " && unzip -o " + pkg_path_;

  return RunCommand(command, err_msg);
}

std::string Firmware::QueryAppVersion() {
  // TODO(oystein) : Prep the meta file
  // TODO(porce) : Read from meta file
  const std::string kDefaultAppVersion = "0.0.8";
  app_version_ = kDefaultAppVersion;
  return kDefaultAppVersion;
}

std::string Firmware::QueryBootloaderVersion() {
  // TODO(oystein) : Prep the meta file
  // TODO(porce) : Read from meta file
  const std::string kDefaultBootloaderVersion = "0.0.8";
  bootloader_version_ = kDefaultBootloaderVersion;
  return kDefaultBootloaderVersion;
}

bool Firmware::ParseManifest(std::string* app_ver,
                             std::string* bootloader_ver,
                             std::string* hw_rev) const {
  const char* kKeyAppVer = "mv2_app.version";
  const char* kKeyBootloaderVer = "mv2_bootloader.version";
  const char* kKeyHwRev = "hw_rev";

  *app_ver = "";
  *bootloader_ver = "";
  *hw_rev = "";

  std::ifstream file(manifest_path_.c_str());

  while (file.good()) {
    std::string line;
    getline(file, line);

    std::string key, value;
    if (!ParseManifestLine(line, &key, &value)) {
      continue;
    }

    if (key == kKeyAppVer) {
      *app_ver = value;
    } else if (key == kKeyBootloaderVer) {
      *bootloader_ver = value;
    } else if (key == kKeyHwRev) {
      *hw_rev = value;
    }
  }

  // Integrity test
  return (*app_ver != "" && *bootloader_ver != "" && *hw_rev == "");
}

void Firmware::ShowInfo() const {
  std::string app_ver, bootloader_ver, hw_rev;
  ParseManifest(&app_ver, &bootloader_ver, &hw_rev);

  std::cout << std::endl
            << "Firmware package:" << std::endl
            << "  dir:           " << pkg_dir_ << std::endl
            << "  bootloader:    " << bootloader_ver << std::endl
            << "  app:           " << app_ver << std::endl
            << "  hw_rev:        " << hw_rev << std::endl;
}

}  // namespace huddly
