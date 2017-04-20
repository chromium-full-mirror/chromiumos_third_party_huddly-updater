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
const char kWorkDir[] = "/tmp/huddly/";
const char kFirmwareAppPath[] = "/tmp/huddly/bin/huddly.bin";
const char kFirmwareBootloaderPath[] = "/tmp/huddly/bin/huddly_boot.bin";

// Test code. TODO(porce): Delete this line
const char kHuddlyPkgPath[] = "/tmp/huddly.pkg";
// const char kHuddlyPkgPath[] = "/lib/firmware/huddly.pkg";

std::string Firmware::GetFirmwarePath() const {
  // TODO(porce): Replace with gflags.
  return kHuddlyPkgPath;
}

Firmware::Firmware() {
  firmware_path_ = GetFirmwarePath();
  // TODO(porce): Replace std::out with logging, with policy.
  std::cout << ".. using firmware package: " << firmware_path_ << std::endl;
}
Firmware::~Firmware() {}

bool Firmware::IsReady(std::string* err_msg) const {
  if (!HasFile(kHuddlyPkgPath)) {
    *err_msg = "firmware pkg not ready: " + std::string(kHuddlyPkgPath);
    return false;
  }
  std::cout << 12 << std::endl;
  if (!Uncompress(firmware_path_, err_msg)) {
    return false;
  }
  if (!HasFile(kFirmwareAppPath)) {
    *err_msg = "firmware app not ready: " + std::string(kFirmwareAppPath);
    return false;
  }
  if (!HasFile(kFirmwareBootloaderPath)) {
    *err_msg = "firmware bootloader not ready: " +
               std::string(kFirmwareBootloaderPath);
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
  std::cout << ".. Uncompressing " << file_path << " on " << kWorkDir
            << std::endl;
  // TODO(porce): Make this line configurable. Make it softer.
  // TODO(porce): Consider using standard library.
  std::string command = "mkdir -p " + std::string(kWorkDir) + " && cd " +
                        std::string(kWorkDir) + " && unzip -o " +
                        std::string(kHuddlyPkgPath);
  if (!RunCommand(command, err_msg)) {
    return false;
  }
  return true;
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

}  // namespace huddly
