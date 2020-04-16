// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils.h"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iterator>
#include <sstream>

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

std::string StripAnsiSgrCodes(const std::string& in) {
  std::string out;
  auto out_inserter = std::back_inserter(out);

  size_t pos = 0;
  while (pos != std::string::npos) {
    const auto esc_pos = in.find("\x1b", pos);
    if (esc_pos == std::string::npos) {
      // No more escape codes, copy rest of string.
      std::copy(in.begin() + pos, in.end(), out_inserter);
    } else {
      // Escape code found. Copy until first escape character.
      std::copy(in.begin() + pos, in.begin() + esc_pos, out_inserter);
    }
    const auto end = in.find("m", esc_pos);
    // If the end of the control code is not found, break.
    if (end == std::string::npos) {
      break;
    }
    // Start again after the control code.
    pos = end + 1;
  }

  return out;
}

}  // namespace huddly
