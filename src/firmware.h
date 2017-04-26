// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_FIRMWARE_H_
#define SRC_FIRMWARE_H_

#include <string>

namespace huddly {

class Firmware {
 public:
  Firmware();
  ~Firmware();

  bool IsReady(std::string* err_msg) const;
  bool Uncompress(const std::string& file_path, std::string* err_msg) const;

  std::string GetFirmwarePath() const;
  bool HasFile(const std::string& file_path) const;

  std::string app_path() const { return app_path_; }
  std::string app_version() const { return app_version_; }
  std::string bootloader_version() const { return bootloader_version_; }

  std::string QueryAppVersion();
  std::string QueryBootloaderVersion();

 private:
  Firmware(const Firmware&) = delete;
  Firmware& operator=(const Firmware&) = delete;

  std::string firmware_path_;
  std::string app_path_;
  std::string bootloader_version_;
  std::string app_version_;
};

}  // namespace huddly

#endif  // SRC_FIRMWARE_H_
