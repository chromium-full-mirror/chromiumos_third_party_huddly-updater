// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
//#include <base/logging.h>
#include <base/values.h>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

#include "manifest.h"

namespace huddly {

namespace {

const int kAcceptableManifestVersionMin = 2;
const int kAcceptableManifestVersionMax = 2;
const int kManifestVersionUnknown = -1;

}  // namespace

bool Manifest::ParseFile() {
  std::string content = GetFileContent(path_);
  if (content.empty()) {
    std::cout << "..Empty content of file: " << path_.value() << "\n";
    return false;
  }

  // Note: |root_dic| is valid only during |root| is alive.
#ifdef __ANDROID__
  scoped_ptr<base::Value> root =
      base::JSONReader().Read(content, base::JSON_PARSE_RFC);
#else
  auto root = base::JSONReader::Read(content, base::JSON_PARSE_RFC);
#endif

  if (!root) {
    std::cout << "..No root is found in JSON dict in file: " << path_.value()
              << "\n";
    return false;
  }

  if (!root->is_dict()) {
    std::cout << "..Failed to read as dictionary from file: " << path_.value()
              << "\n";
    return false;
  }

  const base::DictValue& dict = root->GetDict();
  manifest_ver_ = ParseManifestVersion(dict);
  if (!IsCompatible(manifest_ver_)) {
    std::cout << "..ParseManifestDict failed: manifest version "
              << manifest_ver_
              << " is incompatible from file: " << path_.value() << "\n";
    return false;
  }

  // Parse strings: Upon internal failures, empty strings are returned.
  ParseHardwareRev(dict, &hw_rev_);
  ParseFirmwareFileVersions(dict, &app_ver_, &boot_ver_);

  return true;
}

int Manifest::ParseManifestVersion(const base::DictValue& dic) {
  // key: manifest_version
  auto manifest_version = dic.FindInt("manifest_version");
  if (!manifest_version.has_value()) {
    std::cout << "..failed to parse manifest_version. Default to "
              << kManifestVersionUnknown << "\n";
    return kManifestVersionUnknown;
  }

  return *manifest_version;
}

bool Manifest::IsCompatible(int manifest_ver) {
  bool result = (kAcceptableManifestVersionMin <= manifest_ver) &&
                (manifest_ver <= kAcceptableManifestVersionMax);

  if (!result) {
    std::cout << "..Manifest version " << manifest_ver
              << " is outside the supported range: ["
              << kAcceptableManifestVersionMin << ", "
              << kAcceptableManifestVersionMax << "]"
              << "\n";
  }
  return result;
}

void Manifest::ParseFirmwareFileVersions(const base::DictValue& dic,
                                         std::string* app_ver,
                                         std::string* boot_ver) {
  *app_ver = "";
  *boot_ver = "";

  // key: files
  const base::ListValue* files_list = dic.FindList("files");
  if (!files_list) {
    std::cout << "failed to parse in finding 'files' key"
              << "\n";
    return;
  }

  for (const auto& elem : *files_list) {
    if (!elem.is_dict()) {
      std::cout << "failed to parse an files element as dictionary"
                << "\n";
      continue;
    }

    const std::string* type = elem.GetDict().FindString("type");
    if (!type) {
      std::cout << "failed to parse type as string";
      continue;
    }

    const base::DictValue* version_dic = elem.GetDict().FindDict("version");
    if (!version_dic) {
      std::cout << "failed to parse version as dictionary";
      continue;
    }

    const base::ListValue* numerical_list =
        version_dic->FindList("numerical");
    if (!numerical_list) {
      std::cout << "failed to parse numerical as list";
      continue;
    }

    std::string numerical_str = ListToStr(*numerical_list, ".");

    if (*type == "mv2_boot") {
      *boot_ver = numerical_str;
    } else if (*type == "mv2_app") {
      *app_ver = numerical_str;
    }
  }
}

std::string Manifest::ListToStr(const base::ListValue& list_val,
                                const std::string& separator) {
  std::string result = "";
  for (const auto& val : list_val) {
    if (!val.is_int()) {
      std::cout << "failed to parse value as integer from list value"
                << "\n";
      continue;
    }
    result.append(std::to_string(val.GetInt()));
    result.append(separator);
  }
  result.pop_back();  // Drop the last separator;

  return result;
}

void Manifest::ParseHardwareRev(const base::DictValue& dic,
                                std::string* hw_rev) {
  *hw_rev = "";

  // key: compatible_hw
  const base::ListValue* compatible_hw_list = dic.FindList("compatible_hw");
  if (!compatible_hw_list) {
    std::cout << "failed to parse in finding 'compatible_hw' key'"
              << "\n";
    return;  // returning empty string |hw_rev|.
  }

  for (const auto& compatible_hw_dic : *compatible_hw_list) {
    if (!compatible_hw_dic.is_dict()) {
      std::cout << "failed to parse 'compatible_hw' element as dictionary"
                << "\n";
      continue;
    }

    const base::ListValue* rev_list =
        compatible_hw_dic.GetDict().FindList("hwrevs");
    if (!rev_list) {
      std::cout << "failed to parse hwrevs as list"
                << "\n";
      continue;
    }

    *hw_rev = ListToStr(*rev_list, ",");
  }
}

std::string Manifest::GetFileContent(const base::FilePath& path) {
  std::string content;
  if (!ReadFileToString(path, &content)) {
    return "";
  }

  return content;
}

void Manifest::Dump() {
  std::cout << "Manifest file: " << path_.value() << "\n";
  std::cout << "Manifest ver : " << manifest_ver_ << "\n";
  std::cout << "App      ver : " << app_ver_ << "\n";
  std::cout << "Boot     ver : " << boot_ver_ << "\n";
  std::cout << "Hardware rev : " << hw_rev_ << "\n";
}

}  // namespace huddly
