// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_MINICAM_DEVICE_H_
#define SRC_MINICAM_DEVICE_H_

#include <stdint.h>
#include <string>

#include "usb_device.h"

namespace huddly {

extern const uint16_t kVendorId;
extern const uint16_t kProductIdApp;
extern const uint16_t kProductIdBootloader;

// Vendor command
enum class VendorRequest : uint8_t {
  VERSION = 0x00,
  REBOOT = 0x01,
  PRODUCT_INFO = 0x02,
  BOOT_MODE = 0x10,
  // UVC_EP_MODE = 0x11,  // No use. Leave for reference.
  HW_REV = 0x12,

  // MV2 commands: MV2 for Movidius Myriad Version 2
  MV2_ALIVE_AND_WRITE = 0x40,
  MV2_WRITE = 0x41,
  MV2_READ = 0x42,
  MV2_ISALIVE = 0x48,
};

enum class BootMode : uint8_t {
  // Note: 0x00 is used for APP, not for UNKNOWN.
  APP = 0x00,         // Known as "spi" mode in old huddly reference code.
  BOOTLOADER = 0x01,  // Known as "usb" mode in old huddly reference code.
  BOOTLOADER_STICKY = 0x02,  // Known as "usb-sticky".
  UNKNOWN = 0x10,
};

enum class StreamMode : uint16_t {
  SINGLE = 0x0000,
  DUAL = 0x0001,
  TRIPLE = 0x0002,
  WRITE_ENABLE = 0x8eb0,
};

uint16_t BootModeToProductId(BootMode mode);
std::string BootModeStr(BootMode boot_mode);

std::string StreamModeToStr(StreamMode stream_mode);

class MinicamDevice : public go::UsbDevice {
 public:
   MinicamDevice(uint16_t vendor_id, uint16_t product_id,
                 std::string usb_path = "", std::string usb_serial = "");
   ~MinicamDevice();

   bool CheckIfExists();
   bool RebootInMode(BootMode boot_mode, std::string *err_msg);
   bool Reboot(std::string *err_msg);
   bool WriteImage(int data_len, uint8_t *data, std::string *err_msg,
                   bool dry_run = false) const;

   // Show hardware revision and the firmware versions of bootloader and app
   // firmware.
   void ShowInfo();

   // Works in any mode.
   bool GetHwRevision(std::string *hw_rev_str, std::string *err_msg) const;

   // Works in Bootloader mode.
   std::string QueryFirmwareVersion() const;

   // Works in App mode only.
   bool GetVersion(std::string *app_ver, std::string *bootloader_ver);

   bool GetStreamMode(StreamMode *stream_mode) const;
   bool SetStreamMode(StreamMode stream_mode, std::string *err_msg) const;

   bool GetForceHighSpeedMode(bool *force_high_speed,
                              std::string *err_msg) const;
   bool SetForceHighSpeedMode(const bool force_high_speed,
                              std::string *err_msg) const;

 private:
  MinicamDevice(const MinicamDevice&) = delete;
  MinicamDevice& operator=(const MinicamDevice&) = delete;

  bool VendorRead(VendorRequest vendor_request,
                  uint32_t data_len,
                  uint8_t* data,
                  std::string* err_msg) const;
  bool VendorWrite(VendorRequest vendor_request,
                   uint32_t data_len,
                   uint8_t* data,
                   std::string* err_msg) const;

  bool GetBootMode(BootMode* boot_mode, std::string* err_msg) const;
  bool SetBootMode(BootMode boot_mode, std::string* err_msg) const;
  bool IsBootloaderMode() const;

  bool AllocateBuffer(uint32_t size, std::string* err_msg) const;
  bool UploadImage(uint32_t data_len,
                   uint8_t* data,
                   std::string* err_msg) const;

  void ShowProgress(uint32_t offset, uint32_t data_len) const;

  bool Mv2Commit(uint32_t offet, uint32_t size, std::string* err_msg) const;
  bool Mv2WriteChunk(uint32_t offset,
                     uint32_t data_len,
                     uint8_t* data,
                     std::string* err_msg) const;
  bool Mv2Write(uint32_t data_len, uint8_t* data, std::string* err_msg) const;
  bool Mv2Read(uint32_t data_len, uint8_t* data, std::string* err_msg) const;
  bool Mv2Command(uint32_t cmd_len,
                  uint8_t* cmd,
                  uint32_t retries,
                  std::string* err_msg) const;
  bool Mv2GetAck() const;
  std::string FormatVersion(uint8_t* data) const;
};

}  // namespace huddly

#endif  // SRC_MINICAM_DEVICE_H_
