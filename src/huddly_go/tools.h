// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_TOOLS_H_
#define SRC_TOOLS_H_

#include <string>

namespace huddly {

bool RunCommand(const std::string& cmd, std::string* output);
std::string UsbIdToString(uint16_t vendor_id, uint16_t product_id);

uint32_t LittleEndianUint8ArrayToUint32(uint8_t* array);

uint64_t GetNowMilliSec();
void SleepMilliSec(uint32_t millisec);

bool GetFileSize(const std::string& img_path,
                 uint32_t* file_size,
                 std::string* err_msg);
uint32_t ReadFileToArray(const std::string& img_path,
                         uint32_t data_len,
                         uint8_t* data,
                         std::string* err_msg);

#ifdef __ANDROID__
bool UncompressZip(const char* dir_path,
                   const char* file_path,
                   std::string* err_msg);

int NftwDeleteFile(const char* fpath,
                   const struct stat* sb,
                   int typeFlag,
                   struct FTW* ftwbuf);
#endif

}  // namespace huddly

#endif  // SRC_TOOLS_H_
