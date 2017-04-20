// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <iostream>

#include "tools.h"

namespace huddly {

const int kShellCmdOutBufSize = 128;

bool RunCommand(const std::string& cmd, std::string* output) {
  char buff[kShellCmdOutBufSize];
  std::string result;

  std::cout << "[CMD] " << cmd << std::endl;
  std::FILE* pipe(popen(cmd.c_str(), "r"));
  if (!pipe) {
    *output = "failed to open pipe";
    return false;
  }

  while (fgets(buff, sizeof(buff), pipe) != nullptr) {
    result += buff;
  }
  std::cout << "[OUT] " << std::endl << result << std::endl;
  pclose(pipe);
  *output = result;
  return true;
}

std::string UsbIdToString(uint16_t vendor_id, uint16_t product_id) {
  char buffer[10];
  snprintf(buffer, sizeof(buffer), "%04x:%04x", vendor_id, product_id);
  return buffer;
}

}  // namespace huddly
