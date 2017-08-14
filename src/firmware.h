// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_FIRMWARE_H_
#define SRC_FIRMWARE_H_

#include <string>

namespace huddly {

class Firmware {
 public:
  Firmware(const std::string& pkg_path = "");
  ~Firmware();

  bool IsReady(std::string* err_msg) const;
  bool Uncompress(const std::string& file_path, std::string* err_msg) const;

  std::string GetFirmwarePath() const;
  bool HasFile(const std::string& file_path) const;

  std::string app_path() const { return app_path_; }
  std::string bootloader_path() const { return bootloader_path_; }
  std::string app_version() const { return app_version_; }
  std::string bootloader_version() const { return bootloader_version_; }

  bool ParseManifestJSON(std::string* app_ver,
                         std::string* bootloader_ver,
                         std::string* hw_rev) const;

  // Show firmware versions and the supported hardware revisions
  // of firmware package file.
  void ShowInfo() const;

  // Check if manual upgrade work flow in action or not.
  bool IsManualUpgrade() const { return !pkg_path_.empty(); }

 private:
  Firmware(const Firmware&) = delete;
  Firmware& operator=(const Firmware&) = delete;

  // TODO(crbug.com/719567): Use base::FilePath.
  std::string pkg_path_;  // Absolute path to file "huddly.pkg".
                          // eg. /lib/firmware/huddly/huddly.pkg

  std::string pkg_dir_;  // Directory is required to have following files:
                         // bin/huddly.bin
                         // bin/huddly_boot.bin
                         // manifest.json
                         // manifest.txt

  std::string app_path_;
  std::string bootloader_path_;
  std::string manifest_path_;
  std::string bootloader_version_;
  std::string app_version_;
};

}  // namespace huddly

#endif  // SRC_FIRMWARE_H_
