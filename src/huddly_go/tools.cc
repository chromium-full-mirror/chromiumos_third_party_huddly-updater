// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <fstream>

#ifdef __ANDROID__
#include <fcntl.h>     // open flags
#include <ftw.h>       // nftw
#include <libgen.h>    // dirname
#include <sys/stat.h>  // mkdir
#include <ziparchive/zip_archive.h>
#endif

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
    LOG(INFO) << "[CMD] " << cmd;
  std::FILE* pipe(popen(cmd.c_str(), "r"));
  if (!pipe) {
    *output = "failed to open pipe";
    return false;
  }

  while (fgets(buff, sizeof(buff), pipe) != nullptr) {
    result += buff;
  }
  if (verbose)
    LOG(INFO) << "[OUT] " << result;
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

#ifdef __ANDROID__

/**
 * Returns a pointer to a string  representing the parent directory
 * of the given path
 *
 * Note: Required call to {@code free()} to prevent memory leak
 *
 * @param path - the full path
 * @return an allocated char pointer of the name of the parent directory
 */
static inline char* parentname(const char* path) {
  size_t slen = strlen(dirname(path)) + 1;
  char* sub_path = (char*)malloc(slen);
  std::strncpy(sub_path, path, slen);
  sub_path[slen - 1] = (char)'\0';
  return sub_path;
}

/**
 * Given an absolute directory path creates the necessary parent directories
 *
 * Note: Absolute Path required
 *
 * @param path - absolute path
 * @param mode - masked File permissions bits for new directory
 * @return |0| if successful, |-1| otherwise.
 */
static int mkdirs(const char* path, mode_t mode) {
  if (!path) {
    errno = EINVAL;
    return -1;
  }

  // check if path already exists
  std::ifstream f(path);
  if (f.good()) {
    return 0;
  }

  // Check path is root
  if (strlen(path) == 1 && path[0] == '/') {
    return 0;
  }

  char* sub_path = parentname(path);
  mkdirs(sub_path, mode);
  free(sub_path);

  return mkdir(path, mode);
}

/**
 * Creates parent hierarchy if it does not exist
 *
 * Note: Absolute Path required
 *
 * @param path - absolute path
 * @param mode - masked File permissions bits for created directories
 * @return |0| if successful, |-1| otherwise.
 */
static int CheckFileParents(const char* path, mode_t mode) {
  char* sub_path = parentname(path);
  int ret = mkdirs(sub_path, mode);
  free(sub_path);
  return ret;
}

/**
 * Uncompresses a zip file to a given directory
 *
 * @param path - absolute destination directory path
 * @param mode - absolute path to zip file
 * @param err_msg - error message
 * @return |true| if successful, |false| otherwise.
 */
bool UncompressZip(const char* dir_path,
                   const char* file_path,
                   std::string* err_msg) {
  ZipArchiveHandle handle;
  void* iteration_cookie;
  ZipEntry data;
  ZipString name;

  if (OpenArchive(file_path, &handle) != 0) {
    *err_msg = std::string("Unable to open archive: ") + "\t" + strerror(errno);
    return false;
  }

  if (StartIteration(handle, &iteration_cookie, nullptr, nullptr) != 0) {
    *err_msg = std::string("Unable to uncompress: ") + "\t" + strerror(errno);
    return false;
  }

  while (!Next(iteration_cookie, &data, &name)) {
    std::string file_entry_name(name.name, name.name + name.name_length);
    std::string path = std::string(dir_path) + file_entry_name;
    CheckFileParents(path.c_str(), 0700);

    int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);

    if (fd < 0) {
      *err_msg = std::string("Unable to create temp file: ") + path + "\t" +
                 strerror(errno);
      return false;
    }

    int32_t res = ExtractEntryToFile(handle, &data, fd);
    close(fd);

    if (res != 0) {
      *err_msg =
          std::string("Unable to extract file: ") + "\t" + strerror(errno);
      return false;
    }
  }

  EndIteration(iteration_cookie);
  CloseArchive(handle);

  return true;
}

int NftwDeleteFile(const char* fpath,
                   const struct stat* sb,
                   int typeFlag,
                   struct FTW* ftwbuf) {
  int ret = remove(fpath);
  if (ret != 0) {
    LOG(INFO) << "Error deleting: " << fpath << strerror(errno);
  }
  return ret;
}

#endif

}  // namespace huddly
