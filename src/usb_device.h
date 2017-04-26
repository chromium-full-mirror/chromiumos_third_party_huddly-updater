// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_USB_DEVICE_H_
#define SRC_USB_DEVICE_H_

#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <string>

namespace huddly {

class UsbDevice {
 public:
  UsbDevice();
  UsbDevice(uint16_t vendor_id, uint16_t product_id);
  ~UsbDevice();

  // TODO(porce): Remove first Setup() or vary the names without overladin.
  bool Setup(std::string* err_msg);
  bool Setup(uint16_t vendor_id, uint16_t product_id, std::string* err_msg);
  bool Teardown(std::string* err_msg);

  bool ControlTransfer(uint8_t request_type,
                       uint8_t request,
                       uint32_t address,
                       uint32_t data_len,
                       uint8_t* data,
                       std::string* err_msg) const;

  std::string GetId() const;
  std::string ErrCodeToErrMsg(int err_code) const;

 private:
  UsbDevice(const UsbDevice&) = delete;
  UsbDevice& operator=(const UsbDevice&) = delete;

  bool GetDevHandle(std::string* err_msg);
  bool ClaimInterface(std::string* err_msg);
  bool ReleaseInterface(std::string* err_msg);
  int GetInterfaceNumber() const;

  uint16_t vendor_id_;
  uint16_t product_id_;
  int interface_number_;  // bInterfaceNumber
  bool was_kernel_driver_active_;
  bool is_claimed_;
  std::string version_;

  libusb_context* context_;
  libusb_device* dev_;
  libusb_device_handle* dev_handle_;
};

}  // namespace huddly

#endif  // SRC_USB_DEVICE_H_
