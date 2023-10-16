// Copyright 2017 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "minicam_device.h"

#include <base/logging.h>
#include <endian.h>
#include <iostream>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "stdint.h"

#include "tools.h"
#include "usb_device.h"

namespace huddly {

const uint16_t kVendorId = 0x2bd9;
const uint16_t kProductIdApp = 0x0011;
const uint16_t kProductIdBootloader = 0x0010;
const uint16_t kProductIdInvalid = 0x0000;
// USB 3.0 max packet size (512 bytes) minus vendor command header.
const uint32_t kChunkSizeBytes = 500;
const uint64_t kMv2ReadTimeout = 500;
const uint64_t kTypicalCommitTimeMs = 30000;

// Handling firmware response from huddly.
const uint8_t kAppVersionOffset = 1;
const uint8_t kBootloaderVersionOffset = 5;

uint16_t BootModeToProductId(BootMode mode) {
  switch (mode) {
    case BootMode::APP:
      return kProductIdApp;
    case BootMode::BOOTLOADER:
      return kProductIdBootloader;
    case BootMode::BOOTLOADER_STICKY:
    case BootMode::UNKNOWN:
    default:
      return kProductIdInvalid;
  }
}

std::string StreamModeToStr(StreamMode stream_mode) {
  switch (stream_mode) {
    case StreamMode::SINGLE:
      return "Single";
    case StreamMode::DUAL:
      return "Dual";
    case StreamMode::TRIPLE:
      return "Triple";
    case StreamMode::WRITE_ENABLE:
      return "Write Enable";
    default:
      uint16_t unknown_val = static_cast<uint16_t>(stream_mode);
      char return_string[15];
      snprintf(return_string, sizeof(return_string), "UNKNOWN:0x%04x",
               unknown_val);
      return return_string;
  }
}

MinicamDevice::MinicamDevice(uint16_t vendor_id,
                             uint16_t product_id,
                             std::string usb_path,
                             std::string usb_serial)
    : go::UsbDevice(vendor_id, product_id, usb_path, usb_serial) {}

MinicamDevice::~MinicamDevice() {}

bool MinicamDevice::CheckIfExists() {
  bool result = false;
  std::string err_msg;
  if (!Exists(&result, &err_msg)) {
    // Error while checking. This does not mean check failed and it turned out
    // that the device does not exist.
    err_msg += ".. failed to check if the device exists or not";
    LOG(ERROR) << err_msg;
  }
  return result;
}

bool MinicamDevice::RebootInMode(BootMode boot_mode, std::string* err_msg) {
  LOG(INFO) << ".. setting boot mode: " << BootModeStr(boot_mode);

  if (!SetBootMode(boot_mode, err_msg)) {
    *err_msg += ".. failed to rebooting in mode: ";
    *err_msg += BootModeStr(boot_mode);

    return false;
  }

  LOG(INFO) << ".. done setting boot mode. rebooting ";

  if (!Reboot(err_msg)) {
    *err_msg += ".. failed to reboot in mode: ";
    *err_msg += BootModeStr(boot_mode);
    return false;
  }
  return true;
}

bool MinicamDevice::Reboot(std::string* err_msg) {
  // TODO(porce): Should we use 0x03(all) or 0x02 (MV2)?
  uint8_t data = 0x03;  // For all devices.

  if (!VendorWrite(VendorRequest::REBOOT, sizeof(data), &data, err_msg)) {
    *err_msg += ".. failed to reboot";
    return false;
  }

  // No return value check. The device is just lost because of the reboot.
  // It is normal to have a return value of false.
  Teardown(err_msg);
  return true;
}

bool MinicamDevice::GetForceHighSpeedMode(bool *force_high_speed,
                                          std::string *err_msg) const {
  // TODO(crbug.com/968383): Reimplement using MessagePack to encode/decode

  // The encoded key, value pair to search for
  const uint8_t kForceHighSpeedEntry[] = {0xad, 0x66, 0x6f, 0x72, 0x63,
                                          0x65, 0x2d, 0x68, 0x73, 0x2d,
                                          0x6f, 0x6e, 0x6c, 0x79, 0xc3};
  uint16_t data_len = 0;
  std::vector<uint8_t> data;
  *force_high_speed = false;

  // Query the size in bytes of the Product Info Map
  if (!VendorRead(VendorRequest::PRODUCT_INFO,
                  static_cast<uint32_t>(sizeof(data_len)),
                  reinterpret_cast<uint8_t *>(&data_len), err_msg)) {
    *err_msg += ".. failed in querying map length";
    return false;
  }

  if (data_len <= 0) {
    *err_msg += ".. invalid map length returned";
    return false;
  }

  // Query the entire Product Info Map
  data.resize(data_len);
  if (!VendorRead(VendorRequest::PRODUCT_INFO, static_cast<uint32_t>(data_len),
                  data.data(), err_msg)) {
    *err_msg += ".. failed to query product information";
    return false;
  }

  *force_high_speed =
      (std::search(std::begin(data), std::end(data),
                   std::begin(kForceHighSpeedEntry),
                   std::end(kForceHighSpeedEntry)) != std::end(data));
  return true;
}

bool MinicamDevice::SetForceHighSpeedMode(const bool force_high_speed,
                                          std::string *err_msg) const {
  // TODO(crbug.com/968383): Reimplement using MessagePack to encode property
  // Encoded 'force-hs-only' using https://kawanet.github.io/msgpack-lite/
  uint8_t property[] = {0x81, 0xad, 0x66, 0x6f, 0x72, 0x63, 0x65, 0x2d,
                        0x68, 0x73, 0x2d, 0x6f, 0x6e, 0x6c, 0x79, 0x00};

  // We use 0xc0 instead of 0xc2 to clear the entry. This is in an attempt to
  // keep the dataset small
  uint8_t map_value = force_high_speed ? 0xc3 : 0xc0;
  property[sizeof(property) - 1] = map_value;

  if (!VendorWrite(VendorRequest::PRODUCT_INFO, sizeof(property), property,
                   err_msg)) {
    *err_msg += ".. failed to set usb mode";
    return false;
  }

  return true;
}

bool MinicamDevice::WriteImage(int data_len,
                               uint8_t* data,
                               std::string* err_msg,
                               bool dry_run) const {
  LOG(INFO) << ".. begin allocating buffer";
  if (!AllocateBuffer(data_len, err_msg)) {
    *err_msg += ".. failed to allocate buffer";
    LOG(ERROR) << *err_msg;
    return false;
  }
  LOG(INFO) << ".. end allocating buffer. begin uploading image " << data_len
            << " bytes"
            << "    ";
  if (!UploadImage(data_len, data, err_msg)) {
    *err_msg += ".. failed to upload image";
    LOG(ERROR) << *err_msg;
    return false;
  }

  if (dry_run) {
    LOG(INFO) << ".. dry run mode. Skip committing";
    return true;
  }

  LOG(INFO) << ".. end uploading. begin committing... ";
  if (!Mv2Commit(0, data_len, err_msg)) {
    *err_msg += ".. failed to commit";
    LOG(INFO) << *err_msg;
    return false;
  }

  LOG(INFO) << ".. end committing";
  return true;
}

void MinicamDevice::ShowInfo() {
  std::string err_msg;
  std::string hw_rev;
  std::string bootloader_ver;
  std::string app_ver;

  if (!GetHwRevision(&hw_rev, &err_msg)) {
    std::cerr << err_msg << std::endl;
    return;
  }
  GetVersion(&app_ver, &bootloader_ver);

  // Bypass the logging facility. libchrome logging does not honor stdout
  // properly. Instead it goes to stderr.
  std::cout << "Camera Peripheral:" << std::endl;
  std::cout << "  bootloader:  " << bootloader_ver.c_str() << std::endl;
  std::cout << "  app:         " << app_ver.c_str() << std::endl;
  std::cout << "  hw_rev:      " << hw_rev.c_str() << std::endl;
}

bool MinicamDevice::GetHwRevision(std::string* hw_rev_str,
                                  std::string* err_msg) const {
  uint8_t hw_rev;
  if (!VendorRead(VendorRequest::HW_REV, static_cast<uint32_t>(sizeof(hw_rev)),
                  &hw_rev, err_msg)) {
    *err_msg += ".. failed in querying hardware revision";
    *hw_rev_str = "unknown";
    return false;
  }

  *hw_rev_str = std::to_string(hw_rev);
  return true;
}

std::string MinicamDevice::QueryFirmwareVersion() const {
  // For Bootloader mode only.
  const int kVersionBytes = 3;
  uint8_t data[kVersionBytes];
  std::string err_msg;

  if (!VendorRead(VendorRequest::VERSION, kVersionBytes, data, &err_msg)) {
    LOG(ERROR) << ".. failed in vendor reading: " << err_msg;
    return "unknown";  //
  }

  char buffer[kVersionBytes * 2 + 1];
  snprintf(buffer, sizeof(buffer), "%02x%02x%02x", data[0], data[1], data[2]);
  return buffer;
}

bool MinicamDevice::GetVersion(std::string* app_ver,
                               std::string* bootloader_ver) {
  // This API is known to work only in App mode.
  std::string err_msg;

  // UVC command method queries bootloader and app versions together.
  const uint8_t kRequestType = 0xa1;  // UVC command.
  const uint8_t kRequest = 0x81;      // GET_CUR.
  const uint16_t kValue = 0x1300;     // XU number 19
  const uint16_t kIndex = 0x0400;     // Unit number 4, interace 0.
  const uint32_t kDataLen = 8;
  uint8_t data[kDataLen];

  bool ret = ControlTransfer(kRequestType, kRequest, kValue, kIndex, kDataLen,
                             data, &err_msg);
  if (!ret) {
    // Possible errors are Pipe or IO related.
    LOG(ERROR) << err_msg;
    *app_ver = "unknown";
    *bootloader_ver = "unknown";
    return false;
  }

  // Decimal representation.
  *app_ver = FormatVersion(data + kAppVersionOffset);
  *bootloader_ver = FormatVersion(data + kBootloaderVersionOffset);

  return true;
}

bool MinicamDevice::VendorRead(VendorRequest vendor_request,
                               uint32_t data_len,
                               uint8_t* data,
                               std::string* err_msg) const {
  const uint8_t kRequestType = 0xc0;
  const uint8_t request = static_cast<uint8_t>(vendor_request) & 0xff;
  const uint16_t kAddress = 0x00;
  uint16_t value = kAddress & 0xffff;
  uint16_t index = (kAddress >> 16) & 0xffff;

  const uint8_t kUsbRetryLimit = 20;
  for (uint8_t retry = 0; retry < kUsbRetryLimit; retry++) {
    *err_msg = "";  // Suppress previous errors within a single retry set.
    if (ControlTransfer(kRequestType, request, value, index, data_len, data,
                        err_msg)) {
      if (retry > 0) {
        LOG(INFO) << ".. vendor read successful after " << retry << " retries";
      }
      return true;
    }
    // Possible errors are Pipe or IO related.
    SleepMilliSec(500);
  }
  *err_msg += ".. failed in vendor read: " + std::to_string(request);
  *err_msg += " (retried " + std::to_string(kUsbRetryLimit) + ")";
  return false;
}

bool MinicamDevice::VendorWrite(VendorRequest vendor_request,
                                uint32_t data_len,
                                uint8_t* data,
                                std::string* err_msg) const {
  const uint8_t kRequestType = 0x40;
  const uint8_t request = static_cast<uint8_t>(vendor_request);
  uint16_t address = 0x00;
  if (vendor_request == VendorRequest::REBOOT) {
    address = 0x01;
  }
  uint16_t value = address & 0xffff;
  uint16_t index = (address >> 16) & 0xffff;

  const uint8_t kUsbRetryLimit = 20;
  for (uint8_t retry = 0; retry < kUsbRetryLimit; retry++) {
    *err_msg = "";  // Suppress previous errors within a single retry set.
    if (ControlTransfer(kRequestType, request, value, index, data_len, data,
                        err_msg)) {
      if (retry > 0) {
        LOG(INFO) << ".. vendor write successful after " << retry << " retries";
      }
      return true;
    }

    // Possible errors are Pipe or IO related.
    SleepMilliSec(500);
  }

  *err_msg += ".. failed in vendor write: " + std::to_string(request);
  *err_msg += " (retried " + std::to_string(kUsbRetryLimit) + ")";
  return false;
}

bool MinicamDevice::GetStreamMode(StreamMode* stream_mode) const {
  const uint8_t kRequestType = 0xa1;  // UVC command.
  const uint8_t kRequest = 0x81;      // GET_CUR.
  const uint16_t kValue = 0x0100;     // XU_CONTROL value 0x0001
  const uint16_t kIndex = 0x0400;     // Unit number 4, interface 0
  const uint32_t kDataLen = 2;
  uint8_t data[kDataLen];
  uint16_t mode_val;
  std::string err_msg;

  bool ret = ControlTransfer(kRequestType, kRequest, kValue, kIndex, kDataLen,
                             data, &err_msg);

  if (!ret) {
    LOG(ERROR) << err_msg;
    return false;
  }

  // First treat as little endian, convert to host endian after.
  mode_val = (data[1] << 8) | data[0];
  *stream_mode = static_cast<StreamMode>(le16toh(mode_val));

  return true;
}

bool MinicamDevice::SetStreamMode(StreamMode stream_mode,
                                  std::string* err_msg) const {
  const uint8_t kRequestType = 0x21;
  const uint8_t kRequest = 0x01;
  const uint16_t kValue = 0x0100;
  const uint16_t kIndex = 0x0400;
  const uint32_t kDataLen = 2;
  uint8_t data[kDataLen];
  uint16_t mode_val = htole16(static_cast<uint16_t>(StreamMode::WRITE_ENABLE));

  data[0] = mode_val;
  data[1] = mode_val >> 8;
  bool ret = ControlTransfer(kRequestType, kRequest, kValue, kIndex, kDataLen,
                             data, err_msg);

  if (!ret) {
    *err_msg += " ... failed to write enable camera";
    return false;
  }

  mode_val = htole16(static_cast<uint16_t>(stream_mode));
  data[0] = mode_val;
  data[1] = mode_val >> 8;
  ret = ControlTransfer(kRequestType, kRequest, kValue, kIndex, kDataLen, data,
                        err_msg);

  if (!ret) {
    *err_msg += "... failed to set stream configuration";
    return false;
  }

  return true;
}

bool MinicamDevice::GetBootMode(BootMode* boot_mode,
                                std::string* err_msg) const {
  uint8_t data;
  // TODO(crbug.com/968383): This implementation is likely incorrect and
  // should be rewritten using MessagePack or using a different request
  if (!VendorRead(VendorRequest::PRODUCT_INFO,
                  static_cast<uint32_t>(sizeof(data)), &data, err_msg)) {
    *err_msg += ".. failed in querying boot mode";
    return false;
  }

  *boot_mode = static_cast<BootMode>(data);
  return true;
}

bool MinicamDevice::SetBootMode(BootMode boot_mode,
                                std::string* err_msg) const {
  uint8_t data;
  data = static_cast<uint8_t>(boot_mode);
  if (!VendorWrite(VendorRequest::BOOT_MODE, sizeof(data), &data, err_msg)) {
    *err_msg += ".. failed to set boot mode";
    return false;
  }
  return true;
}

bool MinicamDevice::IsBootloaderMode() const {
  std::string err_msg;
  BootMode boot_mode = BootMode::UNKNOWN;
  if (!GetBootMode(&boot_mode, &err_msg)) {
    err_msg += ".. failed to test if it is in Bootloader mode";
    LOG(ERROR) << err_msg;
    return false;  // Fallback to assume App mode, which is most natural.
  }
  return boot_mode == BootMode::BOOTLOADER;
}

std::string BootModeStr(BootMode boot_mode) {
  switch (boot_mode) {
    case BootMode::APP:
      return "APP";
    case BootMode::BOOTLOADER:
      return "BOOTLOADER";
    case BootMode::BOOTLOADER_STICKY:
      return "BOOTLOADER_STICKY";
    case BootMode::UNKNOWN:
      return "UNKNOWN";
    default:
      return "INVALID";
  }
}

bool MinicamDevice::AllocateBuffer(uint32_t size, std::string* err_msg) const {
  // TODO(porce): CRC check.
  uint32_t retries = 5;

  const uint32_t kCmdLen = 5;
  uint8_t cmd[kCmdLen];
  uint32_t size_le = htole32(size);
  cmd[0] = 'a';
  memcpy(cmd + 1, &size_le, sizeof(size_le));

  if (!Mv2Command(kCmdLen, cmd, retries, err_msg)) {
    *err_msg += "failed to allocate buffer";
    return false;
  }
  return true;
}

bool MinicamDevice::UploadImage(uint32_t data_len,
                                uint8_t* data,
                                std::string* err_msg) const {
  for (uint32_t offset = 0; offset < data_len; offset += kChunkSizeBytes) {
    uint32_t chunk_size = kChunkSizeBytes;
    if (offset + chunk_size > data_len) {
      // May happen at the tail of the data
      chunk_size = (data_len - offset);
    }

    ShowProgress(offset, data_len);

    // Mv2WriteChunk has a retry logic inside.
    // No need to make this function pesistent.
    if (!Mv2WriteChunk(offset, chunk_size, data + offset, err_msg)) {
      // TODO(porce): How to recover in the middle?
      char msg[100];
      snprintf(msg, sizeof(msg),
               "\n!! failed to write in offset %u data_len %u chunksize %u",
               offset, data_len, chunk_size);
      *err_msg += msg;

      LOG(INFO) << "Mv2WriteChunk failed. Probably the device is lost. "
                   "Was it unplugged?";
      LOG(INFO) << "Stop uploading image. Start bailout sequence..";
      return false;
    }
  }

  ShowProgress(data_len, data_len);  // To show 100%
  return true;
}

void MinicamDevice::ShowProgress(uint32_t offset, uint32_t data_len) const {
  // Humble version of showing ShowProgress.
  // Show percentage progress only upon a change in 10% unit.
  // TODO(porce): Beautify this.
  if (data_len == 0)
    return;

  static uint32_t percent_prev = 0;
  uint32_t percent = 100 * offset / data_len;
  if (percent % 10 == 0 && percent != percent_prev) {
    LOG(INFO) << percent << "%..";
    percent_prev = percent;
  }

  if (percent_prev == 100) {
    percent_prev = 0;  // reset for next
  }
}

// Mv2Commit(): Call flow in this function is in question.
// The right flow depends on the behavior of the device.
// TODO(porce): Discuss with vendor.
bool MinicamDevice::Mv2Commit(uint32_t offset,
                              uint32_t size,
                              std::string* err_msg) const {
  const uint32_t kCmdLen = 9;
  uint8_t cmd[kCmdLen];
  uint32_t offset_le = htole32(offset);
  uint32_t size_le = htole32(size);
  cmd[0] = 'W';
  memcpy(cmd + 1, &offset_le, sizeof(offset_le));
  memcpy(cmd + 1 + sizeof(offset_le), &size_le, sizeof(size_le));
  if (!Mv2Command(kCmdLen, cmd, 1, err_msg)) {
    LOG(INFO)
        << ".. failed to get ack in mv2_command. But proceed with the upgrade";
    // keep going.
  }

  SleepMilliSec(2000);  // 2 sec.

  uint64_t kTimeout = 2 * kTypicalCommitTimeMs;
  uint64_t expiry = GetNowMilliSec() + kTimeout;

  while (GetNowMilliSec() < expiry) {
    const uint32_t kDataLen = 5;
    uint8_t data[kDataLen];

    if (!Mv2Read(kDataLen, data, err_msg)) {
      // failed to read the status. Mv2Read itself has an embedded retry logic.
      // Do not need to repeat extra retry. Retrun failure.
      LOG(INFO) << "Mv2Read failed. Start bailout sequence..";
      return false;
    }

    uint8_t status = data[0];
    uint32_t pos = LittleEndianUint8ArrayToUint32(data + 1);

    switch (status) {
      case 'e':
        // TODO(porce): ShowProgress.
        LOG(INFO) << "[e]";
        break;
      case 'E':
        // TODO(porce): ShowProgress.
        LOG(INFO) << "[E]";
        break;
      case 'w':
        // TODO(porce): ShowProgress.
        LOG(INFO) << "[w]";
        break;
      case 'W':
        LOG(INFO) << "[W]";
        return true;
        break;
      case 'f':
        LOG(INFO) << ".. Erase failure";
        return false;
      case 'F':
        LOG(INFO) << ".. Flash failure" << std::endl;
        return false;
      case 'V':
        LOG(INFO) << ".. Verification failed" << std::endl;
        return false;
      default:
        char msg[100];
        snprintf(msg, sizeof(msg),
                 ".. received unexpected commit status: %c, %d", status, pos);
        *err_msg += msg;
        return false;
    }
    SleepMilliSec(1000);  // A way to suppress. The status is not queued.
  }
  *err_msg += ".. failed to commit. timed out: ";
  *err_msg += std::to_string(kTimeout / 1000) + " sec";
  return false;
}

// MV2 methods
bool MinicamDevice::Mv2WriteChunk(uint32_t offset,
                                  uint32_t data_len,
                                  uint8_t* data,
                                  std::string* err_msg) const {
  uint32_t offset_le = htole32(offset);
  uint32_t buffer_len = 1 + sizeof(offset_le) + data_len;

  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_len]);

  *buffer.get() = 'w';
  memcpy(buffer.get() + 1, &offset_le, sizeof(offset_le));
  memcpy(buffer.get() + 1 + sizeof(offset_le), data, data_len);

  bool result = Mv2Write(buffer_len, buffer.get(), err_msg);

  if (result) {
    return true;
  }
  *err_msg += "\n!! failed to mv2 write at offset: " + std::to_string(offset);
  return false;
}

bool MinicamDevice::Mv2Write(uint32_t data_len,
                             uint8_t* data,
                             std::string* err_msg) const {
  VendorRequest req = VendorRequest::MV2_WRITE;

  if (!VendorWrite(req, data_len, data, err_msg)) {
    *err_msg += ".. failed in Mv2Write";
    return false;
  }
  return true;
}

bool MinicamDevice::Mv2Read(uint32_t data_len,
                            uint8_t* data,
                            std::string* err_msg) const {
  uint64_t expiry = GetNowMilliSec() + kMv2ReadTimeout;

  while (GetNowMilliSec() < expiry) {
    if (VendorRead(VendorRequest::MV2_READ, data_len, data, err_msg)) {
      return true;
    }
  }
  *err_msg += ".. Mv2Read() timed out";
  return false;
}

bool MinicamDevice::Mv2Command(uint32_t cmd_len,
                               uint8_t* cmd,
                               uint32_t retries,
                               std::string* err_msg) const {
  // Note Mv2Command implements an outer loop of retries,
  // on top of the Mv2Write() and Mv2GetAck()'s internal
  // retry mechanism (which is implemented within ControlTransfer().
  // It is of a question if this level of retries is necessary or not, knowing
  // the outer loop will increase the timeout drastically. Since this solely
  // depends on the vendor's device characteristics, a change of this logic
  // should be consulted with the vendor.

  for (uint32_t retry = 0; retry < retries; retry++) {
    if (!Mv2Write(cmd_len, cmd, err_msg)) {
      continue;  // continue to retry.
    }

    if (Mv2GetAck()) {
      if (retry > 0) {
        // Treat this as success, but pass this for deep debugging purpose.
        *err_msg +=
            ".. Mv2Command() succeeds with retries: " + std::to_string(retry);
      }
      return true;
    }
  }
  *err_msg += ".. Mv2Command() with retries: " + std::to_string(retries);
  return false;
}

bool MinicamDevice::Mv2GetAck() const {
  uint8_t data;
  std::string err_msg;

  // Only return value (boolean) matters for this purpose.
  return Mv2Read(sizeof(data), &data, &err_msg);
}

std::string MinicamDevice::FormatVersion(uint8_t* data) const {
  return std::to_string(data[2]) + "." + std::to_string(data[1]) + "." +
         std::to_string(data[0]);
}

}  // namespace huddly
