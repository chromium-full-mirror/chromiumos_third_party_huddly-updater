// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SRC_HUDDLY_HPK_USB_H_
#define SRC_HUDDLY_HPK_USB_H_

#include <base/macros.h>
#include <libusb.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace huddly {

class Usb;

class UsbEndpoint {
public:
  UsbEndpoint() {}
  explicit UsbEndpoint(const libusb_endpoint_descriptor& descriptor);

  int address() const { return address_; }
  int max_size() const { return max_size_; }

private:
  int address_ = -1;
  int max_size_ = -1;
};

class UsbDevice {
public:
  explicit UsbDevice(const Usb* usb, libusb_device* dev);
  ~UsbDevice();

  bool Open();
  void Close();
  std::unique_ptr<libusb_config_descriptor,
                  std::function<void(libusb_config_descriptor*)>>
  GetActiveConfigDescriptor();
  uint16_t GetVendorId() const;
  uint16_t GetProductId() const;
  bool ClaimInterface(uint8_t interface_number);
  bool ReleaseInterface(uint8_t interface_number);
  bool BulkWrite(const UsbEndpoint& endpoint, const std::vector<uint8_t> data,
                 const unsigned int timeout_ms);
  bool BulkRead(const UsbEndpoint& endpoint, const unsigned int timeout_ms,
                std::vector<uint8_t>* data);
  bool WaitForDetach(const unsigned int timeout_ms);

  const Usb* GetUsb() { return usb_; }

private:
  const Usb* usb_ = nullptr;
  libusb_device* dev_ = nullptr;
  libusb_device_descriptor device_descriptor_ = {};
  libusb_device_handle* devh_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UsbDevice);
};

class UsbDeviceList {
public:
  static std::unique_ptr<UsbDeviceList> Create(libusb_context* ctx);
  ~UsbDeviceList();

  bool Valid() const;
  libusb_device** begin();
  libusb_device** end();

private:
  UsbDeviceList() {}

  ssize_t count_;
  libusb_device** devices_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceList);
};

class Usb {
public:
  static std::unique_ptr<Usb> Create();
  ~Usb();

  std::unique_ptr<UsbDevice> FindDevice(uint16_t vid, uint16_t pid) const;
  std::unique_ptr<UsbDevice> WaitForDevice(uint16_t vid, uint16_t pid,
                                           int timeout_ms) const;
  bool DeviceExists(libusb_device* device) const;
  bool WaitForDeviceDetach(libusb_device* device, int timeout_ms) const;

private:
  Usb() {}
  libusb_context* ctx_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Usb);
};

}  // namespace huddly

#endif  // SRC_HUDDLY_HPK_USB_H_
