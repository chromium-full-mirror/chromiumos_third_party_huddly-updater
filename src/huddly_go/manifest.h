// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_MANIFEST_H_
#define SRC_MANIFEST_H_

#include <base/files/file_path.h>
#include <base/values.h>
#include <string>

namespace huddly {

class Manifest {
 public:
  explicit Manifest(const base::FilePath& path) : path_(path) {}
  Manifest() = default;
  Manifest(const Manifest&) = delete;
  Manifest& operator=(const Manifest&) = delete;

  ~Manifest() = default;

  bool ParseFile();
  int ParseManifestVersion(const base::DictValue& dic);
  bool IsCompatible(int manifest_ver);
  void ParseHardwareRev(const base::DictValue& dic, std::string* hw_rev);
  void ParseFirmwareFileVersions(const base::DictValue& dic,
                                 std::string* app_ver,
                                 std::string* boot_ver);

  std::string GetFileContent(const base::FilePath& path);
  std::string ListToStr(const base::ListValue& list_val,
                        const std::string& separator);
  void Dump();  // show the result of parsing.

  void set_path(const base::FilePath& path) { path_ = path; }
  base::FilePath path() const { return path_; }
  std::string app_ver() const { return app_ver_; }
  std::string boot_ver() const { return boot_ver_; }
  std::string hw_rev() const { return hw_rev_; }

 private:
  base::FilePath path_;

  int manifest_ver_;
  std::string app_ver_;
  std::string boot_ver_;
  std::string hw_rev_;
};

}  // namespace huddly

#endif  // SRC_MANIFEST_H_
