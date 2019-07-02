// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils.h"

#include <iomanip>
#include <sstream>

namespace huddly {

std::string Uint8ToHexString(uint8_t value) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(2) << value;
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

}  // namespace huddly
