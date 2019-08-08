// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_HUDDLY_HPK_UTILS_H_
#define SRC_HUDDLY_HPK_UTILS_H_

#include <cstdint>
#include <string>
#include <vector>

namespace huddly {

std::string Uint8ToHexString(uint8_t value);
std::string Uint16ToHexString(uint16_t value);
std::string UsbVidPidString(uint16_t vid, uint16_t pid);
int VersionCompare(const std::vector<uint32_t>& v1,
                   const std::vector<uint32_t>& v2);

}  // namespace huddly

#endif  //  SRC_HUDDLY_HPK_UTILS_H_
