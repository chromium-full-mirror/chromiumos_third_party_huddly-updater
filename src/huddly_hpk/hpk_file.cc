// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hpk_file.h"

#include <base/files/file_util.h>
#include <base/json/json_reader.h>

namespace huddly {

static const int kExpectedFirmwareVersionLength = 3;

std::unique_ptr<HpkFile> HpkFile::Create(const base::FilePath& path,
                                         std::string* error_msg) {
  if (!base::PathExists(path)) {
    *error_msg = "File '" + path.MaybeAsASCII() + "' does not exist.";
    return nullptr;
  }

  std::string file_contents;
  if (!base::ReadFileToString(path, &file_contents)) {
    *error_msg = "Failed to read '" + path.MaybeAsASCII() + "'";
    return nullptr;
  }

  return Create(file_contents, error_msg);
}

std::unique_ptr<HpkFile> HpkFile::Create(const std::string& file_contents,
                                         std::string* error_msg) {
  auto instance = std::unique_ptr<HpkFile>(new HpkFile());
  if (!instance->Setup(file_contents, error_msg)) {
    return nullptr;
  }
  return instance;
}

bool HpkFile::Setup(const std::string& file_contents, std::string* error_msg) {
  file_contents_ = file_contents;
  if (!ExtractManifestData(error_msg)) {
    *error_msg += "\nFailed to extract manifest data.";
    return false;
  }

  if (!ParseJsonManifest(error_msg)) {
    *error_msg += "\nFailed to parse manifest.";
    return false;
  }

  return true;
}

bool HpkFile::ExtractManifestData(std::string* error_msg) {
  static const std::string kMagicSeparator =
      std::string("\0\n--97da1ea4-803a-4979-8e5d-f2aaa0799f4d--\n", 43);
  auto pos = file_contents_.find(kMagicSeparator);
  if (pos == std::string::npos) {
    *error_msg = "Magic separator not found.";
    return false;
  }

  manifest_data_ = file_contents_.substr(0, pos);

  return true;
}

bool HpkFile::ParseJsonManifest(std::string* error_msg) {
  int error_code;
  std::string json_error_msg;
  // Older implementatons of ReadAndReturnError produces scoped_ptr that must be
  // observed, to build on both CrOS and older versions of Android, as such we
  // get the raw pointer and recast to unique_ptr regardless if it is shared_ptr
  // or unique_ptr
  // TODO(crbug/1066447): update to new Read API after libchrome uprev.
  auto root_value = base::JSONReader().ReadAndReturnErrorDeprecated(
      manifest_data_, base::JSON_PARSE_RFC, &error_code, &json_error_msg);
  manifest_root_.reset(std::move(root_value.release()));

  if (!manifest_root_) {
    *error_msg = "Failed to parse json data:\n" + json_error_msg;
    return false;
  }

  // The root of the manifest should always be a dictionary.
  if (!(manifest_root_->GetAsDictionary(&manifest_root_dictionary_))) {
    *error_msg = "Manifest root object is not a dictionary.";
    return false;
  }

  return true;
}

bool HpkFile::GetFirmwareVersionNumeric(std::vector<uint32_t>* version,
                                        std::string* error_msg) const {
  base::ListValue* fw_version;
  if (!(manifest_root_dictionary_->GetList("version.numerical", &fw_version))) {
    *error_msg = "Failed to get firmware version as list.";
    return false;
  }

  if (fw_version->GetSize() != kExpectedFirmwareVersionLength) {
    *error_msg = "Unexpected firmware version length.";
    return false;
  }

  for (int i = 0; i < kExpectedFirmwareVersionLength; i++) {
    int component = -1;
    if (!fw_version->GetInteger(i, &component)) {
      *error_msg = "Failed to get firmware version component as integer.";
      return false;
    }
    version->push_back(component);
  }
  return true;
}

bool HpkFile::GetFirmwareVersionString(std::string* version,
                                       std::string* error_msg) const {
  if (!(manifest_root_dictionary_->GetString("version.app_version", version))) {
    *error_msg = "Failed to get firmware version as string.";
    return false;
  }

  return true;
}

}  // namespace huddly
