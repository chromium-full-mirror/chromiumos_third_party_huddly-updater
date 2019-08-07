// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_USB_DEVICE_H_
#define SRC_USB_DEVICE_H_

#ifdef __ANDROID__
#include <libusb.h>
#else
#include <libusb-1.0/libusb.h>
#endif
#include <stdint.h>
#include <string>

namespace huddly {
namespace go {

class UsbDevice {
 public:
   UsbDevice(uint16_t vendor_id, uint16_t product_id, std::string usb_path = "",
             std::string usb_serial = "");
   ~UsbDevice();

   // Checks if the USB device specified by vendor_id:product_id is present.
   bool Exists(bool *result, std::string *err_msg);
   // Used when a USB device is expected to be attached. Usually called
   // after rebooting with/without switching mode.
   bool WaitForOnline(int timeout_sec, std::string *err_msg);
   // Acquires the libusb device handle, after claiming the interface and
   // detatching the kernel driver if necessary
   bool Setup(std::string *err_msg);
   // Releases the libusb resources including releasing the interface,
   // reattaching the kernel driver, if detached, closes the context.
   bool Teardown(std::string *err_msg);

   bool ControlTransfer(uint8_t request_type, uint8_t request, uint16_t value,
                        uint16_t index, uint32_t data_len, uint8_t *data,
                        std::string *err_msg) const;

   std::string GetId() const;
   std::string GetSerialNumber() const;
   std::string ErrCodeToErrMsg(int err_code) const;

 private:
  UsbDevice(const UsbDevice&) = delete;
  UsbDevice& operator=(const UsbDevice&) = delete;

  libusb_context* GetContext(std::string* err_msg);
  libusb_device* GetDevice(libusb_context* ctx, std::string* err_msg);
  bool DetachKernelDriver(std::string* err_msg);
  bool ReattachKernelDriver(std::string* err_msg);

  bool ClaimInterface(std::string* err_msg);
  bool ReleaseInterface(std::string* err_msg);
  int GetInterfaceNumber() const;

  uint16_t vendor_id_;
  uint16_t product_id_;
  std::string usb_path_;
  std::string usb_serial_number_;
  int interface_number_;  // bInterfaceNumber
  bool was_kernel_driver_active_;
  bool is_claimed_;
  bool is_setup_;
  std::string version_;

  libusb_context* context_;
  libusb_device* dev_;
  libusb_device_handle* dev_handle_;
};

}  // namespace go
}  // namespace huddly

#endif  // SRC_USB_DEVICE_H_
