// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "usb.h"

#include <base/logging.h>
#include <base/time/time.h>
#include <unistd.h>

#include <algorithm>
#include <functional>

namespace huddly {

namespace {

bool is_usb_path_compatible(libusb_device* dev, const std::string& usb_path) {
  if (usb_path.empty()) {
    // Do not match by given path
    return true;
  }
  const int kBufferSize = 9;
  char path_buffer[kBufferSize];
  if (usb_path.length() < kBufferSize) {
    return false;
  }
  uint8_t bus_number = libusb_get_bus_number(dev);
  uint8_t device_address = libusb_get_device_address(dev);
  snprintf(path_buffer, kBufferSize, "/%03d/%03d", bus_number, device_address);
  return usb_path.compare(usb_path.length() - kBufferSize + 1,
                          std::string::npos, path_buffer) == 0;
}

}  // namespace

UsbEndpoint::UsbEndpoint(const libusb_endpoint_descriptor& descriptor)
    : address_(descriptor.bEndpointAddress),
      max_size_(descriptor.wMaxPacketSize) {}

UsbDevice::UsbDevice(const Usb* usb, libusb_device* dev)
    : usb_(usb), dev_(dev) {
  if (dev_) {
    libusb_ref_device(dev_);
    libusb_get_device_descriptor(dev_, &device_descriptor_);
  }
}

UsbDevice::~UsbDevice() {
  Close();
  if (dev_) {
    libusb_unref_device(dev_);
  }
}

bool UsbDevice::Open() {
  int r = libusb_open(dev_, &devh_);
  if (r != 0 || devh_ == nullptr) {
    LOG(ERROR) << "libusb_open failed: " << libusb_error_name(r);
    return false;
  }
  return true;
}

void UsbDevice::Close() {
  if (devh_) {
    libusb_close(devh_);
  }
  devh_ = nullptr;
}

std::unique_ptr<libusb_config_descriptor,
                std::function<void(libusb_config_descriptor*)>>
UsbDevice::GetActiveConfigDescriptor() {
  libusb_config_descriptor* config;
  int r = libusb_get_active_config_descriptor(dev_, &config);
  if (r) {
    LOG(ERROR) << "libusb_get_active_config_descriptor failed: "
               << libusb_error_name(r);
    return std::unique_ptr<libusb_config_descriptor,
                           std::function<void(libusb_config_descriptor*)>>(
        nullptr, [](libusb_config_descriptor*) {});
  }
  // Use custom deleter in unique_ptr to make sure descriptor is deleted.
  return std::unique_ptr<libusb_config_descriptor,
                         std::function<void(libusb_config_descriptor*)>>(
      config, libusb_free_config_descriptor);
}

uint16_t UsbDevice::GetVendorId() const {
  return device_descriptor_.idVendor;
}

uint16_t UsbDevice::GetProductId() const {
  return device_descriptor_.idProduct;
}

bool UsbDevice::CheckKernelDriverActive(uint8_t interface_number,
                                        bool* driver_active) {
  if (!devh_) {
    LOG(ERROR) << "Device not open";
    return false;
  }

  int r = libusb_kernel_driver_active(devh_, interface_number);
  if (r < 0 || r > 1) {
    LOG(ERROR) << "Invalid return value for libusb_kernel_driver_active: "
               << libusb_error_name(r);
    return false;
  }

  // ret == 1: A kernel driver is active.
  *driver_active = r == 1;

  VLOG(1) << "Kernel driver for interface " << unsigned(interface_number)
          << " was active: " << std::boolalpha << bool(*driver_active);
  return true;
}

bool UsbDevice::DetachKernelDriver(uint8_t interface_number) {
  if (!devh_) {
    LOG(ERROR) << "Device not open";
    return false;
  }

  int r = libusb_detach_kernel_driver(devh_, interface_number);
  if (r == 0) {
    VLOG(1) << "Kernel driver detached for interface # "
            << unsigned(interface_number);
    return true;
  }

  LOG(ERROR) << "libusb_detach_kernel_driver failed: " << libusb_error_name(r);
  return false;
}

bool UsbDevice::ReattachKernelDriver(uint8_t interface_number) {
  if (!devh_) {
    LOG(ERROR) << "Device not open";
    return false;
  }

  int r = libusb_attach_kernel_driver(devh_, interface_number);
  if (r != 0) {
    LOG(ERROR) << "libusb_attach_kernel_driver failed: "
               << libusb_error_name(r);
    return false;
  }

  VLOG(1) << "Kernel driver reattached for interface # "
          << unsigned(interface_number);
  return true;
}

bool UsbDevice::ClaimInterface(uint8_t interface_number) {
  if (!devh_) {
    LOG(ERROR) << "Device not open";
    return false;
  }
  int r = libusb_claim_interface(devh_, interface_number);
  if (r) {
    LOG(ERROR) << "libusb_claim_interface failed: " << libusb_error_name(r);
    return false;
  }
  return true;
}

bool UsbDevice::ReleaseInterface(uint8_t interface_number) {
  if (!devh_) {
    LOG(ERROR) << "Device not open";
    return false;
  }
  int r = libusb_release_interface(devh_, interface_number);
  if (r) {
    LOG(ERROR) << "libusb_release_interface failed: " << libusb_error_name(r);
    return false;
  }
  return true;
}

bool UsbDevice::BulkWrite(const UsbEndpoint& endpoint,
                          const std::vector<uint8_t> data,
                          const unsigned int timeout_ms) {
  if (!devh_) {
    LOG(ERROR) << "Device not open";
    return false;
  }
  int total_transferred = 0;
  do {
    int transferred;
    auto r = libusb_bulk_transfer(
        devh_, endpoint.address(),
        const_cast<uint8_t*>(&data[0] + total_transferred),
        data.size() - total_transferred, &transferred, timeout_ms);
    if (r) {
      LOG(ERROR) << "libusb_bulk_transfer failed: " << libusb_error_name(r);
      return false;
    }
    total_transferred += transferred;
  } while (total_transferred < data.size());
  return true;
}

// This function returns up to the packet size of the endpoint. This reason for
// this limitation is documented  at
// http://libusb.sourceforge.net/api-1.0/libusb_packetoverflow.html
bool UsbDevice::BulkRead(const UsbEndpoint& endpoint,
                         const unsigned int timeout_ms,
                         std::vector<uint8_t>* data) {
  if (!devh_) {
    LOG(ERROR) << "Device not open";
    return false;
  }
  data->assign(endpoint.max_size(), 0);
  int transferred;
  int r = libusb_bulk_transfer(devh_, endpoint.address(), &(*data)[0],
                               data->size(), &transferred, timeout_ms);
  if (r) {
    LOG(ERROR) << "libusb_bulk_transfer failed: " << libusb_error_name(r);
    return false;
  }

  data->resize(transferred);
  return true;
}

bool UsbDevice::WaitForDetach(const unsigned int timeout_ms) {
  return usb_->WaitForDeviceDetach(dev_, timeout_ms);
}

std::unique_ptr<UsbDeviceList> UsbDeviceList::Create(libusb_context* ctx) {
  auto object = std::unique_ptr<UsbDeviceList>((new UsbDeviceList()));
  int count = libusb_get_device_list(ctx, &object->devices_);
  if (count < 0 || object->devices_ == nullptr) {
    LOG(ERROR) << "libusb_get_device_list failed: " << libusb_error_name(count);
    return nullptr;
  }
  object->count_ = count;
  return object;
}

UsbDeviceList::~UsbDeviceList() {
  libusb_free_device_list(devices_, 1);
}

libusb_device** UsbDeviceList::begin() {
  return devices_;
}

libusb_device** UsbDeviceList::end() {
  return devices_ + count_;
}

std::unique_ptr<Usb> Usb::Create() {
  auto object = std::unique_ptr<Usb>(new Usb());
  int r = libusb_init(&object->ctx_);
  if (r) {
    LOG(ERROR) << "libusb_init failed: " << libusb_error_name(r);
    return nullptr;
  }
  return object;
}

Usb::~Usb() {
  libusb_exit(ctx_);
}

std::unique_ptr<UsbDevice> Usb::FindDevice(uint16_t vid,
                                           uint16_t pid,
                                           const std::string& usb_path) const {
  auto device_list = UsbDeviceList::Create(ctx_);

  auto dev_it = std::find_if(
      std::begin(*device_list), std::end(*device_list),
      [vid, pid, usb_path](libusb_device* dev) {
        libusb_device_descriptor dev_descr;
        auto r = libusb_get_device_descriptor(dev, &dev_descr);
        if (r) {
          LOG(WARNING) << "Error getting device descriptor: "
                       << libusb_error_name(r);
          return false;
        }

        if (!is_usb_path_compatible(dev, usb_path)) {
          // USB path mismatch
          return false;
        }

        if (dev_descr.idVendor == vid && dev_descr.idProduct == pid) {
          return true;
        }
        return false;
      });

  if (dev_it != std::end(*device_list)) {
    return std::make_unique<UsbDevice>(this, *dev_it);
  }
  return nullptr;
}

std::unique_ptr<UsbDevice> Usb::WaitForDevice(uint16_t vid,
                                              uint16_t pid,
                                              const std::string& usb_path,
                                              int timeout_ms) const {
  const auto start = base::TimeTicks::Now();
  while ((base::TimeTicks::Now() - start).InMilliseconds() < timeout_ms) {
    auto device = FindDevice(vid, pid, usb_path);
    if (device) {
      return device;
    }
    usleep(100 * 1000);
  }

  return nullptr;
}

bool Usb::DeviceExists(libusb_device* device) const {
  auto device_list = UsbDeviceList::Create(ctx_);
  return std::find(std::begin(*device_list), std::end(*device_list), device) !=
         std::end(*device_list);
}

bool Usb::WaitForDeviceDetach(libusb_device* device, int timeout_ms) const {
  const auto start = base::TimeTicks::Now();
  while (DeviceExists(device)) {
    if ((base::TimeTicks::Now() - start).InMilliseconds() > timeout_ms) {
      return false;
    }
    usleep(100 * 1000);
  }
  return true;
}

}  // namespace huddly
