// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils.h"

#include <iomanip>
#include <sstream>
#include <cassert>

namespace huddly {

std::string Uint8ToHexString(uint8_t value) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(2)
      << static_cast<int>(value);
  return oss.str();
}

std::string Uint16ToHexString(const uint16_t value) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(4) << value;
  return oss.str();
}

std::string UsbVidPidString(uint16_t vid, uint16_t pid) {
  return Uint16ToHexString(vid) + ":" + Uint16ToHexString(pid);
}

int VersionCompare(const std::vector<uint32_t>& v1,
                   const std::vector<uint32_t>& v2) {
  assert(v1.size() == v2.size());
  for (auto i = 0u; i < v1.size(); i++) {
    if (v1[i] < v2[i]) {
      return -1;
    } else if (v1[i] > v2[i]) {
      return 1;
    }
  }
  return 0;
}

}  // namespace huddly
