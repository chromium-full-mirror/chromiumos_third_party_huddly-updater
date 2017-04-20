// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_USB_DEVICE_H_
#define SRC_USB_DEVICE_H_

#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <string>

namespace huddly {

// Vendor command
enum class VendorRequest {
  VERSION = 0x00,
  REBOOT = 0x01,
  PRODUCTINFO = 0x02,
  BOOTMODE = 0x10,
  UVCEPMODE = 0x11,
  HWREV = 0x12,
  MV2ALIVEANDWRITE = 0x40,
  MV2WRITE = 0x41,
  MV2READ = 0x42,
  MVISALIVE = 0x48,
};

class UsbDevice {
 public:
  UsbDevice(uint16_t vendor_id, uint16_t product_id);
  ~UsbDevice();

  bool Setup(std::string* err_msg);
  bool Teardown(std::string* err_msg);

  std::string GetId() const;
  std::string ErrCodeToErrMsg(int err_code) const;
  std::string QueryFirmwareVersion();

  /*  TODO(porce): implement
    bool Reboot(std::string *err_msg);
    bool Detach();
    bool Attach();
    bool QueryBootloaderVer(int *version);
    bool QueryAppVer(int *version);
  */

 private:
  UsbDevice(const UsbDevice&) = delete;
  UsbDevice& operator=(const UsbDevice&) = delete;

  bool GetDevHandle(std::string* err_msg);
  bool ClaimInterface(std::string* err_msg);
  bool ReleaseInterface(std::string* err_msg);
  int GetInterfaceNumber() const;

  bool VendorRead(VendorRequest vendor_request, int data_len, uint8_t* data,
                  std::string* err_msg) const;
  bool ControlTransfer(int request_type, VendorRequest vendor_request,
                       int address, int data_len, uint8_t* data,
                       std::string* err_msg) const;

  libusb_context* context_;
  libusb_device* dev_;
  libusb_device_handle* dev_handle_;

  uint16_t vendor_id_;
  uint16_t product_id_;
  int interface_number_;  // bInterfaceNumber
  bool was_kernel_driver_active_;
  bool is_claimed_;
  std::string version_;
};

}  // namespace huddly

#endif  // SRC_USB_DEVICE_H_
