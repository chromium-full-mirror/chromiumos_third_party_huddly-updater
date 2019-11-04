// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils.h"

#include <iomanip>
#include <sstream>
#include <cassert>

namespace huddly {

NumericVersion::NumericVersion() : NumericVersion({0, 0, 0}) {}

NumericVersion::NumericVersion(std::vector<uint32_t> version)
    : version_(version) {
  assert(version.size() == kVersionLength);
}

std::string Uint8ToHexString(uint8_t value) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(2)
      << static_cast<int>(value);
  return oss.str();
}

bool operator==(const NumericVersion& lhs, const NumericVersion& rhs) {
  return lhs.version_ == rhs.version_;
}

bool operator!=(const NumericVersion& lhs, const NumericVersion& rhs) {
  return !operator==(lhs, rhs);
}
bool operator<(const NumericVersion& lhs, const NumericVersion& rhs) {
  const auto& v1 = lhs.version_;
  const auto& v2 = rhs.version_;
  assert(v1.size() == v2.size());
  for (auto i = 0u; i < v1.size(); i++) {
    if (v1[i] < v2[i]) {
      return true;
    } else if (v1[i] > v2[i]) {
      return false;
    }
  }
  return false;
}

bool operator>(const NumericVersion& lhs, const NumericVersion& rhs) {
  return operator<(rhs, lhs);
}

bool operator<=(const NumericVersion& lhs, const NumericVersion& rhs) {
  return !operator>(lhs, rhs);
}

bool operator>=(const NumericVersion& lhs, const NumericVersion& rhs) {
  return !operator<(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, const NumericVersion& version) {
  const auto& vec = version.version_;
  os << vec.at(0) << "." << vec.at(1) << "." << vec.at(2);
  return os;
}

std::string Uint16ToHexString(const uint16_t value) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(4) << value;
  return oss.str();
}

std::string UsbVidPidString(uint16_t vid, uint16_t pid) {
  return Uint16ToHexString(vid) + ":" + Uint16ToHexString(pid);
}

}  // namespace huddly
