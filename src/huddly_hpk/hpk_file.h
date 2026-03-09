// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_HUDDLY_HPK_HPK_FILE_H_
#define SRC_HUDDLY_HPK_HPK_FILE_H_

#include <base/files/file_path.h>
#include <base/values.h>
#include <memory>
#include <string>
#include <vector>

namespace huddly {
class HpkFile {
 public:
  static std::unique_ptr<HpkFile> Create(const base::FilePath& path,
                                         std::string* error_msg);
  static std::unique_ptr<HpkFile> Create(const std::string& file_contents,
                                         std::string* error_msg);
  const std::string& GetContents() const { return file_contents_; }
  const std::string& GetManifestData() const { return manifest_data_; }
  bool GetFirmwareVersionNumeric(std::vector<uint32_t>* version,
                                 std::string* error_msg) const;
  bool GetFirmwareVersionString(std::string* version,
                                std::string* error_msg) const;

 private:
  HpkFile() {}
  bool Setup(const std::string& file_contents, std::string* error_msg);
  bool ExtractManifestData(std::string* error_msg);
  bool ParseJsonManifest(std::string* error_msg);

  std::string file_contents_;
  std::string manifest_data_;
  base::DictValue manifest_root_dictionary_;
  std::vector<uint32_t> firmware_version_;
};

}  // namespace huddly

#endif  // SRC_HUDDLY_HPK_HPK_FILE_H_
