// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>

#include <algorithm>
#include <string>

#include "tools.h"
#include "usb_device.h"

namespace huddly {
namespace go {

const int kStrBufSize = 200;

UsbDevice::UsbDevice(uint16_t vendor_id, uint16_t product_id,
                     std::string usb_path, std::string usb_serial)
    : vendor_id_(vendor_id), product_id_(product_id), usb_path_(usb_path),
      usb_serial_number_(usb_serial), interface_number_(0),
      was_kernel_driver_active_(false), is_claimed_(false), is_setup_(false),
      context_(nullptr), dev_(nullptr), dev_handle_(nullptr) {}

UsbDevice::~UsbDevice() {
  std::string err_msg;
  if (!Teardown(&err_msg)) {
    LOG(ERROR) << ".. error in teardown: " << err_msg;
  }
}

bool UsbDevice::Exists(bool* result, std::string* err_msg) {
  // Note: there are two different kinds of boolean values.
  //  - Return value is for status: success or failure.
  //  - existence check result is stored to *result.
  // This is in order to support the error propagation, if any.

  libusb_context* ctx = GetContext(err_msg);
  if (ctx == nullptr) {
    *err_msg += ".. failed to check if exists: " + GetId();
    *result = false;  // Default result
    return false;
  }

  libusb_device* dev = GetDevice(ctx, err_msg);
  *result = (dev != nullptr);
  libusb_exit(ctx);
  return true;  // API operation succeeded.
}

bool UsbDevice::WaitForOnline(int timeout_sec, std::string* err_msg) {
  uint64_t begin = GetNowMilliSec();
  uint64_t expiry = begin + timeout_sec * 1000;

  while (GetNowMilliSec() < expiry) {
    bool result = false;
    if (!Exists(&result, err_msg)) {
      // API operation failed. Retry.
      continue;
    }
    // API succeeded. Check the result.
    if (result) {
      uint64_t elapsed = (GetNowMilliSec() - begin) / 1000;
      LOG(INFO) << ".. found after waiting for " << elapsed << " sec.";
      return true;
    }
    // Suppress error and continue;
    *err_msg = "";  // Suppress
    huddly::SleepMilliSec(1000);
  }

  *err_msg += ".. cannot find. Timed out(sec): ";
  *err_msg += std::to_string(timeout_sec);
  return false;
}

bool UsbDevice::Setup(std::string* err_msg) {
  // TODO(porce): Instead of calling Teardown() manually, use unique_ptr.
  if (is_setup_) {  // Already setup somehow.
    // Probably the caller tries to reuse the object. Honor it.
    Teardown(err_msg);
  }
  // Set context
  context_ = GetContext(err_msg);
  if (context_ == nullptr) {
    *err_msg +=
        ".. failed to initialize libusb_context .. failed to setup " + GetId();
    Teardown(err_msg);  // Escape sequence. No return check.
    return false;
  }

  dev_ = GetDevice(context_, err_msg);
  if (dev_ == nullptr) {
    *err_msg += ".. failed to setup " + GetId();
    Teardown(err_msg);  // Escape sequence. No return check.
    return false;
  }

  // Get device handle
  int ret = libusb_open(dev_, &dev_handle_);
  if (ret != 0) {
    *err_msg += ".. failed to get device handle: " + ErrCodeToErrMsg(ret);
    *err_msg += ".. failed to setup " + GetId();
    Teardown(err_msg);  // Escape sequence. No return check.
    return false;
  }

  interface_number_ = GetInterfaceNumber();

  if (!DetachKernelDriver(err_msg)) {
    *err_msg += ".. failed to setup " + GetId();
    Teardown(err_msg);  // Escape sequence. No return check.
    return false;
  }

  if (!ClaimInterface(err_msg)) {
    *err_msg += ".. failed to setup " + GetId();
    Teardown(err_msg);  // Escape sequence. No return check.
    return false;
  }

  is_setup_ = true;
  return true;
}  // namespace huddly

bool UsbDevice::Teardown(std::string* err_msg) {
  // Multiple steps in tear down process.
  // Even if one step fails, the next step continues.
  // Error messages are appended.

  bool ret_release = ReleaseInterface(err_msg);
  bool ret_detach = ReattachKernelDriver(err_msg);

  if (dev_handle_) {
    libusb_close(dev_handle_);
    dev_handle_ = nullptr;
  }
  if (dev_) {
    libusb_unref_device(dev_);
    dev_ = nullptr;
  }
  if (context_) {
    libusb_exit(context_);
    context_ = nullptr;
  }

  // Reset POD that are not handled in above subroutines.
  vendor_id_ = 0;
  product_id_ = 0;
  interface_number_ = 0;
  is_setup_ = false;

  return ret_release && ret_detach;
}

bool UsbDevice::ControlTransfer(uint8_t request_type,
                                uint8_t request,
                                uint16_t value,
                                uint16_t index,
                                uint32_t data_len,
                                uint8_t* data,
                                std::string* err_msg) const {
  const int kUsbReadWriteTimeOut = 1000;  // msec

#ifdef DEV_DEBUG_USBMON
  {
    // USB Mon Debug.
    printf(
        "[DBG] LIBUSB MON: "
        "reqtype:req:val:idx:dlen :: data :: "
        "%02x %02x %04x %04x %04x :: ",
        request_type, request, value, index, data_len);
    const uint32_t kBytesToPrint = 4;
    uint32_t trunc_len = std::min(data_len, kBytesToPrint);

    for (uint32_t idx = 0; idx < trunc_len; idx++) {
      printf("%02x ", data[idx]);
    }
    printf(" :: devhandle %p\n", dev_handle_);
  }
#endif  // DEV_DEBUG_USBMON

  int ret =
      libusb_control_transfer(dev_handle_, request_type, request, value, index,
                              data, data_len, kUsbReadWriteTimeOut);

  if (ret == static_cast<int>(data_len)) {
    return true;
  }
  if (ret < 0) {
    *err_msg += ".. failed in control transfer: " + ErrCodeToErrMsg(ret);
    return false;
  }

  // Received only part of what was requested.
  char err_msg_buf[kStrBufSize];
  snprintf(
      err_msg_buf, sizeof(err_msg_buf),
      ".. failed to read fully from USB. Want %d bytes. Got %d bytes: request "
      "type %x request %x value %x index %x",
      data_len, ret, request_type, request, value, index);
  *err_msg += err_msg_buf;
  return false;
}

std::string UsbDevice::GetId() const {
  return UsbIdToString(vendor_id_, product_id_);
}

std::string UsbDevice::ErrCodeToErrMsg(int err_code) const {
  char err_msg[kStrBufSize];
  snprintf(err_msg, sizeof(err_msg), "Error code: %s(%d) : %s",
           libusb_error_name(err_code), err_code,
           libusb_strerror(static_cast<enum libusb_error>(err_code)));

  return err_msg;
}

libusb_context* UsbDevice::GetContext(std::string* err_msg) {
  libusb_context* ctx;
  if (libusb_init(&ctx) == 0) {  // success
#if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000106)
    libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);
#else
    libusb_set_debug(ctx, LIBUSB_LOG_LEVEL_INFO);
#endif
    return ctx;
  }

  *err_msg += ".. failed to initialize libusb_context .. failed to setup ";
  return nullptr;
}

static bool is_usb_path_compatible(libusb_device *dev,
                                   const std::string &usb_path) {
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

static bool check_usb_serial_number(libusb_device *usb_device,
                                    uint8_t desc_serial_number,
                                    std::string *serial_number) {
  if (desc_serial_number == 0) {
    // Cannot query serial number from device
    // return true if not searching by serial number i.e is empty
    return serial_number->empty();
  }

  struct libusb_device_handle *dev_handle = nullptr;

  if (libusb_open(usb_device, &dev_handle) < 0) {
    return serial_number->empty();
  }

  char device_serial_number[256];
  memset(device_serial_number, 0, sizeof(device_serial_number));
  int ret = libusb_get_string_descriptor_ascii(
      dev_handle, desc_serial_number, (unsigned char *)device_serial_number,
      sizeof(device_serial_number));

  libusb_close(dev_handle);

  if (serial_number->empty()) {
    // Do not match by serial and update if not previously set
    (*serial_number) += device_serial_number;
    return true;
  }

  if (ret <= 0 || serial_number->compare(device_serial_number) != 0) {
    return false;
  }

  return true;
}

std::string UsbDevice::GetSerialNumber() const { return usb_serial_number_; }

libusb_device* UsbDevice::GetDevice(libusb_context* ctx, std::string* err_msg) {
  libusb_device** dev_list;
  libusb_device* dev = nullptr;

  int list_cnt = libusb_get_device_list(ctx, &dev_list);
  if (list_cnt < 0) {
    *err_msg += ".. failed to get device list: " + ErrCodeToErrMsg(list_cnt);
    LOG(ERROR) << *err_msg;
    return nullptr;
  }

  libusb_device_descriptor desc;
  for (int idx = 0; idx < list_cnt; idx++) {
    libusb_device* dev_tmp = dev_list[idx];

    if (!is_usb_path_compatible(dev_tmp, usb_path_)) {
      // USB path mismatch
      continue;
    }

    int ret = libusb_get_device_descriptor(dev_tmp, &desc);
    if (ret != 0) {
      LOG(ERROR) << ".. failed to get device descriptor: "
                 << ErrCodeToErrMsg(ret);
      // Don't give up yet.
      continue;
    }

    if (desc.idVendor != vendor_id_ || desc.idProduct != product_id_) {
      // Usb VID PID mismatch
      continue;
    }

    // Extra check for multiple device upgrade issue
    if (!check_usb_serial_number(dev_tmp, desc.iSerialNumber,
                                 &usb_serial_number_)) {
      // Serial Number check failed
      continue;
    }

    dev = dev_tmp;
    libusb_ref_device(dev);
    break; // Found. No need to loop further.
  }

  libusb_free_device_list(dev_list, 1);  // unref by 1

  if (dev == nullptr) {
    *err_msg += ".. failed to get device: not found";
  }

  return dev;
}

bool UsbDevice::DetachKernelDriver(std::string* err_msg) {
  int ret = libusb_kernel_driver_active(dev_handle_, interface_number_);
  if (ret < 0) {
    *err_msg += ".. failed to check if the kernel driver is active: " +
                ErrCodeToErrMsg(ret);
    return false;
  }

  if (ret > 1) {
    // Undocumented return value.
    *err_msg += ".. invalid return value from libusb_kernel_driver_active(): ";
    *err_msg += std::to_string(ret);
    return false;
  }

  if (ret == 0) {
    was_kernel_driver_active_ = false;
    return true;
  }

  // ret == 1: A kernel driver is active. Detach it.
  was_kernel_driver_active_ = true;

  ret = libusb_detach_kernel_driver(dev_handle_, interface_number_);
  if (ret == 0) {
    // Successful detach.
    return true;
  }

  *err_msg +=
      ".. failed to detach active kernel driver: " + ErrCodeToErrMsg(ret);
  return false;
}

bool UsbDevice::ReattachKernelDriver(std::string* err_msg) {
  if (!was_kernel_driver_active_) {
    return true;  // No need to reattach.
  }

  if (dev_handle_ == nullptr) {
    // Case of rebooting
    was_kernel_driver_active_ = false;

    return true;
  }
  int ret = libusb_attach_kernel_driver(dev_handle_, interface_number_);
  if (ret != 0) {
    *err_msg +=
        ".. failed to reattach detached kernel driver: " + ErrCodeToErrMsg(ret);
    return false;
  }

  // Reattached.
  was_kernel_driver_active_ = false;
  return true;
}

bool UsbDevice::ClaimInterface(std::string* err_msg) {
  int ret = libusb_claim_interface(dev_handle_, interface_number_);
  is_claimed_ = (ret == 0);
  if (!is_claimed_) {
    *err_msg += ".. failed to claim: " + ErrCodeToErrMsg(ret);
  }
  return is_claimed_;
}

bool UsbDevice::ReleaseInterface(std::string* err_msg) {
  if (!is_claimed_) {
    return true;  // Nothing to relase.
  }

  if (dev_handle_ == nullptr) {
    // Case of rebooting
    is_claimed_ = false;
    return true;
  }
  int ret = libusb_release_interface(dev_handle_, interface_number_);
  if (ret == 0) {
    is_claimed_ = false;
    return true;
  }

  *err_msg += ".. failed to release: " + ErrCodeToErrMsg(ret);
  return false;
}

int UsbDevice::GetInterfaceNumber() const {
  // TODO(porce): put some proper logic..
  // Huddly camera firmware updater uses only interface 0.
  return 0;
}

}  // namespace go
}  // namespace huddly
