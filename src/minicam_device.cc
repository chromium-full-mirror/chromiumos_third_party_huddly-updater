// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <endian.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

#include "minicam_device.h"
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

MinicamDevice::MinicamDevice(uint16_t vendor_id, uint16_t product_id)
    : UsbDevice(vendor_id, product_id) {}

MinicamDevice::~MinicamDevice() {}

bool MinicamDevice::CheckIfExists() {
  bool result = false;
  std::string err_msg;
  if (!Exists(&result, &err_msg)) {
    // Error while checking. This does not mean check failed and it turned out
    // that the device does not exist.
    err_msg += ".. failed to check if the device exists or not";
    std::cout << err_msg << std::endl;
  }
  return result;
}

bool MinicamDevice::RebootInMode(BootMode boot_mode, std::string* err_msg) {
  std::cout << ".. setting boot mode: " << BootModeStr(boot_mode) << std::endl;

  if (!SetBootMode(boot_mode, err_msg)) {
    *err_msg += ".. failed to rebooting in mode: ";
    *err_msg += BootModeStr(boot_mode);

    return false;
  }

  std::cout << ".. done setting boot mode. rebooting " << std::endl;

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

bool MinicamDevice::WriteImage(int data_len,
                               uint8_t* data,
                               std::string* err_msg) const {
  std::cout << ".. allocating buffer" << std::endl;
  if (!AllocateBuffer(data_len, err_msg)) {
    *err_msg += ".. failed to allocate buffer";
    std::cout << *err_msg << std::endl;
    return false;
  }
  std::cout << ".. done allocating buffer. uploading image: ";
  if (!UploadImage(data_len, data, err_msg)) {
    *err_msg += ".. failed to upload image";
    std::cout << *err_msg << std::endl;
    return false;
  }
  std::cout << ".. done uploading. committing" << std::endl;
  if (!Mv2Commit(0, data_len, err_msg)) {
    *err_msg += ".. failed to commit";
    std::cout << *err_msg << std::endl;
    return false;
  }

  std::cout << ".. done committing" << std::endl;
  return true;
}

bool MinicamDevice::GetHwRevision(uint8_t* hw_rev, std::string* err_msg) const {
  if (!VendorRead(VendorRequest::HW_REV, static_cast<uint32_t>(sizeof(*hw_rev)),
                  hw_rev, err_msg)) {
    *err_msg += ".. failed in querying hardware revision";
    *hw_rev = 0;
    return false;
  }

  return true;
}

bool MinicamDevice::VendorRead(VendorRequest vendor_request,
                               uint32_t data_len,
                               uint8_t* data,
                               std::string* err_msg) const {
  const uint8_t kRequestType = 0xc0;
  const uint8_t request = static_cast<uint8_t>(vendor_request);
  const uint16_t kAddress = 0x00;

  const uint8_t kUsbRetryLimit = 20;
  for (uint8_t retry = 0; retry < kUsbRetryLimit; retry++) {
    if (ControlTransfer(kRequestType, request, kAddress, data_len, data,
                        err_msg)) {
      if (retry > 0) {
        std::cout << ".. vendor read successful after " << retry << " retries"
                  << std::endl;
      }
      return true;
    }
    // Possible errors are Pipe or IO related.
    SleepMilliSec(500);
  }
  *err_msg += ".. failed in vendor read: " + std::to_string(request);
  return false;
}

bool MinicamDevice::VendorWrite(VendorRequest vendor_request,
                                uint32_t data_len,
                                uint8_t* data,
                                std::string* err_msg) const {
  const uint8_t kRequestType = 0x40;
  const uint8_t request = static_cast<uint8_t>(vendor_request);
  uint32_t address = 0x00;
  if (vendor_request == VendorRequest::REBOOT) {
    address = 0x01;
  }

  const uint8_t kUsbRetryLimit = 20;
  for (uint8_t retry = 0; retry < kUsbRetryLimit; retry++) {
    if (ControlTransfer(kRequestType, request, address, data_len, data,
                        err_msg)) {
      if (retry > 0) {
        std::cout << ".. vendor write successful after " << retry << " retries"
                  << std::endl;
      }
      return true;
    }

    // Possible errors are Pipe or IO related.
    SleepMilliSec(500);
  }

  *err_msg += ".. failed in vendor write: " + std::to_string(request);
  return false;
}

bool MinicamDevice::GetBootMode(BootMode* boot_mode,
                                std::string* err_msg) const {
  uint8_t data;

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
    std::cout << err_msg << std::endl;
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
  uint32_t sum = 0;
  for (uint32_t offset = 0; offset < data_len; offset += kChunkSizeBytes) {
    uint32_t chunk_size = kChunkSizeBytes;
    if (offset + chunk_size > data_len) {
      // May happen at the tail of the data
      chunk_size = (data_len - offset);
    }

    ShowProgress(offset, chunk_size, data_len);
    if (!Mv2WriteChunk(offset, chunk_size, data + offset, err_msg)) {
      // TODO(porce): How to recover in the middle?
      char msg[100];
      snprintf(msg, sizeof(msg),
               "\n!! failed to write in offset %u data_len %u chunksize %u",
               offset, data_len, chunk_size);
      *err_msg += msg;
      continue;
    }
    sum += chunk_size;
  }

  std::cout << "[Done] wrote " << sum << " bytes" << std::endl;
  return true;
}

void MinicamDevice::ShowProgress(uint32_t offset,
                                 uint32_t chunk_size,
                                 uint32_t data_len) const {
  // Humble version of showing ShowProgress.
  // Show percentage progress only upon a change in 10% unit.
  // TODO(porce): Beautify this.
  if (data_len == 0)
    return;

  static uint32_t percent_prev = 0;
  uint32_t percent = 100 * offset / data_len;
  if (percent % 10 == 0 && percent != percent_prev) {
    std::cout << percent << "%..";
    percent_prev = percent;
  }
}

std::string MinicamDevice::QueryFirmwareVersion() const {
  const int kVersionBytes = 3;
  uint8_t data[kVersionBytes];
  std::string err_msg;

  if (!VendorRead(VendorRequest::VERSION, kVersionBytes, data, &err_msg)) {
    std::cout << ".. failed in vendor reading: " << err_msg << std::endl;
    return "unknown";  //
  }

  char buffer[kVersionBytes * 2 + 1];
  snprintf(buffer, sizeof(buffer), "%02x%02x%02x", data[0], data[1], data[2]);
  return buffer;
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
    std::cout
        << ".. failed to get ack in mv2_command. But proceed with the upgrade"
        << std::endl;
    // keep going.
  }

  SleepMilliSec(2000);  // 2 sec.

  uint64_t kTimeout = 2 * kTypicalCommitTimeMs;
  uint64_t expiry = GetNowMilliSec() + kTimeout;

  while (GetNowMilliSec() < expiry) {
    const uint32_t kDataLen = 5;
    uint8_t data[kDataLen];
    if (!Mv2Read(kDataLen, data, err_msg)) {
      // failed to read the status. Keep trying. Don't give up.
      SleepMilliSec(200);  // 0.2 sec
      continue;
    }

    uint8_t status = data[0];
    uint32_t pos = LittleEndianUint8ArrayToUint32(data + 1);

    switch (status) {
      case 'e':
        // TODO(porce): ShowProgress.
        std::cout << "[e]";
        break;
      case 'E':
        // TODO(porce): ShowProgress.
        std::cout << "Erase done" << std::endl;
        break;
      case 'w':
        // TODO(porce): ShowProgress.
        std::cout << "[w]";
        break;
      case 'W':
        std::cout << ".. Write done" << std::endl;
        return true;
        break;
      case 'f':
        std::cout << ".. Erase failure" << std::endl;
        return false;
      case 'F':
        std::cout << ".. Flash failure" << std::endl;
        return false;
      case 'V':
        std::cout << ".. Verification failed" << std::endl;
        return false;
      default:
        char msg[100];
        snprintf(msg, sizeof(msg),
                 ".. received unexpected commit status: %c, %d", status, pos);
        *err_msg += msg;
        return false;
    }
    SleepMilliSec(200);  // 0.2 sec
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

}  // namespace huddly
