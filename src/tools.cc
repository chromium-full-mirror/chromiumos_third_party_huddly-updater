// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include "tools.h"

namespace huddly {

const int kShellCmdOutBufSize = 128;

bool RunCommand(const std::string& cmd, std::string* output) {
  char buff[kShellCmdOutBufSize];
  std::string result;

  // TODO(crbug.com/719567): Use DLOG(INFO) instead of "if (verbose) std::out".
  bool verbose = false;
#ifdef DEV_DEBUG
  verbose = true;
#endif  // DEV_DEBUG

  if (verbose)
    std::cout << "[CMD] " << cmd << std::endl;
  std::FILE* pipe(popen(cmd.c_str(), "r"));
  if (!pipe) {
    *output = "failed to open pipe";
    return false;
  }

  while (fgets(buff, sizeof(buff), pipe) != nullptr) {
    result += buff;
  }
  if (verbose)
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

uint32_t LittleEndianUint8ArrayToUint32(uint8_t* array) {
  return array[0] | array[1] << 8 | array[2] << 16 | array[3] << 24;
}

uint64_t GetNowMilliSec() {
  struct timeval tp;
  gettimeofday(&tp, nullptr);
  return static_cast<uint16_t>(tp.tv_sec * 1000) +
         static_cast<uint64_t>(tp.tv_usec / 1000);
}

void SleepMilliSec(uint32_t millisec) {
  // Blocking sleep.
  // TODO(porce): research the impact of EINTR and the alternative of nanosleep.
  usleep(millisec * 1000);
}

bool GetFileSize(const std::string& img_path,
                 uint32_t* file_size,
                 std::string* err_msg) {
  // TODO(porce): file_size  - uint32_t or uint64_t
  struct stat file_stat;
  if (stat(img_path.c_str(), &file_stat) < 0) {
    *err_msg += ".. failed to get file size: ";
    *err_msg += strerror(errno);
    return false;
  }
  *file_size = static_cast<uint32_t>(file_stat.st_size);
  return true;
}

uint32_t ReadFileToArray(const std::string& img_path,
                         uint32_t data_len,
                         uint8_t* data,
                         std::string* err_msg) {
  if (data_len == 0 || data == nullptr) {
    *err_msg += ".. failed to read file. Zero data_len or null data";
    return 0;
  }

  std::ifstream fin(img_path, std::ifstream::binary);
  fin.read(reinterpret_cast<char*>(data), data_len);
  return fin.gcount();
}

}  // namespace huddly
