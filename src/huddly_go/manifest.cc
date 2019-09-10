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
      base::JSONReader().Read(content, base::JSON_DETACHABLE_CHILDREN);
#else
  std::unique_ptr<base::Value> root =
      base::JSONReader().Read(content, base::JSON_DETACHABLE_CHILDREN);
#endif

  if (!root) {
    std::cout << "..No root is found in JSON dict in file: " << path_.value()
              << "\n";
    return false;
  }

  base::DictionaryValue* root_dic;
  if (!(root->GetAsDictionary(&root_dic))) {
    std::cout << "..Failed to read as dictionary from file: " << path_.value()
              << "\n";
    return false;
  }

  manifest_ver_ = ParseManifestVersion(root_dic);
  if (!IsCompatible(manifest_ver_)) {
    std::cout << "..ParseManifestDict failed: manifest version "
              << manifest_ver_
              << " is incompatible from file: " << path_.value() << "\n";
    return false;
  }

  // Parse strings: Upon internal failures, empty strings are returned.
  ParseHardwareRev(root_dic, &hw_rev_);
  ParseFirmwareFileVersions(root_dic, &app_ver_, &boot_ver_);

  return true;
}

int Manifest::ParseManifestVersion(base::DictionaryValue* dic) {
  if (!dic) {
    std::cout << "..faield to parse manifest_version: null dic. Default to "
              << kManifestVersionUnknown << "\n";
    return kManifestVersionUnknown;
  }

  // key: manifest_version
  int manifest_version;
  if (!dic->GetInteger("manifest_version", &manifest_version)) {
    std::cout << "..failed to parse manifest_version. Default to "
              << kManifestVersionUnknown << "\n";
    return kManifestVersionUnknown;
  }

  return manifest_version;
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

void Manifest::ParseFirmwareFileVersions(base::DictionaryValue* dic,
                                         std::string* app_ver,
                                         std::string* boot_ver) {
  *app_ver = "";
  *boot_ver = "";

  if (!dic) {
    std::cout << "..ParseFirmwareFileVersions failed: null dic";
    return;
  }

  // key: files
  base::ListValue* files_list = nullptr;
  if (!dic->GetList("files", &files_list)) {
    std::cout << "failed to parse in finding 'files' key"
              << "\n";
    return;
  }

  for (auto& elem : *files_list) {
    base::DictionaryValue* dic = nullptr;
    if (!elem->GetAsDictionary(&dic)) {
      std::cout << "failed to parse an files element as dictionary"
                << "\n";
      continue;
    }

    std::string type;
    if (!dic->GetString("type", &type)) {
      std::cout << "failed to parse type as string";
      continue;
    }

    base::DictionaryValue* version_dic = nullptr;
    if (!dic->GetDictionary("version", &version_dic)) {
      std::cout << "failed to parse version as dictionary";
      continue;
    }

    base::ListValue* numerical_list = nullptr;
    if (!version_dic->GetList("numerical", &numerical_list)) {
      std::cout << "failed to parse numerical as list";
      continue;
    }

    std::string numerical_str = ListToStr(numerical_list, ".");

    if (type == "mv2_boot") {
      *boot_ver = numerical_str;
    } else if (type == "mv2_app") {
      *app_ver = numerical_str;
    }
  }
}

std::string Manifest::ListToStr(base::ListValue* list_val,
                                const std::string& separator) {
  if (!list_val) {
    std::cout << "ListToStr: null list value"
              << "\n";
    return "";
  }

  std::string result = "";
  for (auto& elem : *list_val) {
    int val;
    if (!elem->GetAsInteger(&val)) {
      std::cout << "failed to parse value as integer from list value"
                << "\n";
      continue;
    }
    result.append(std::to_string(val));
    result.append(separator);
  }
  result.pop_back();  // Drop the last separator;

  return result;
}

void Manifest::ParseHardwareRev(base::DictionaryValue* dic,
                                std::string* hw_rev) {
  *hw_rev = "";
  if (!dic) {
    std::cout << "..ParseHardwareRev: null dic"
              << "\n";
    return;
  }

  // key: compatible_hw
  base::ListValue* compatible_hw_list = nullptr;
  if (!dic->GetList("compatible_hw", &compatible_hw_list)) {
    std::cout << "failed to parse in finding 'compatible_hw' key'"
              << "\n";
    return;  // returning empty string |hw_rev|.
  }

  for (auto& elem : *compatible_hw_list) {
    base::DictionaryValue* compatible_hw_dic = nullptr;
    if (!elem->GetAsDictionary(&compatible_hw_dic)) {
      std::cout << "failed to parse 'compatible_hw' element as dictionary"
                << "\n";
      continue;
    }

    base::ListValue* rev_list = nullptr;
    if (!compatible_hw_dic->GetList("hwrevs", &rev_list)) {
      std::cout << "failed to parse hwrevs as list"
                << "\n";
      continue;
    }

    *hw_rev = ListToStr(rev_list, ",");
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
