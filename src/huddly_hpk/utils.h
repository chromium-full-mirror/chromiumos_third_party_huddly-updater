// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_HUDDLY_HPK_UTILS_H_
#define SRC_HUDDLY_HPK_UTILS_H_

#include <cstdint>
#include <string>
#include <ostream>
#include <vector>

namespace huddly {

class NumericVersion {
 public:
  NumericVersion();
  NumericVersion(const NumericVersion&) = default;
  NumericVersion(NumericVersion&&) = default;
  NumericVersion& operator=(const NumericVersion&) = default;
  NumericVersion& operator=(NumericVersion&&) = default;
  explicit NumericVersion(std::vector<uint32_t> version);

  friend bool operator==(const NumericVersion& lhs, const NumericVersion& rhs);
  friend bool operator!=(const NumericVersion& lhs, const NumericVersion& rhs);
  friend bool operator<(const NumericVersion& lhs, const NumericVersion& rhs);
  friend bool operator>(const NumericVersion& lhs, const NumericVersion& rhs);
  friend bool operator<=(const NumericVersion& lhs, const NumericVersion& rhs);
  friend bool operator>=(const NumericVersion& lhs, const NumericVersion& rhs);
  friend std::ostream& operator<<(std::ostream& os,
                                  const NumericVersion& version);

 private:
  static const int kVersionLength = 3;
  std::vector<uint32_t> version_;
};

bool operator==(const NumericVersion& lhs, const NumericVersion& rhs);
bool operator!=(const NumericVersion& lhs, const NumericVersion& rhs);
bool operator<(const NumericVersion& lhs, const NumericVersion& rhs);
bool operator>(const NumericVersion& lhs, const NumericVersion& rhs);
bool operator<=(const NumericVersion& lhs, const NumericVersion& rhs);
bool operator>=(const NumericVersion& lhs, const NumericVersion& rhs);
std::ostream& operator<<(std::ostream& os, const NumericVersion& version);

std::string Uint8ToHexString(uint8_t value);
std::string Uint16ToHexString(uint16_t value);
std::string UsbVidPidString(uint16_t vid, uint16_t pid);

std::string StripAnsiSgrCodes(const std::string& in);

}  // namespace huddly

#endif  //  SRC_HUDDLY_HPK_UTILS_H_
