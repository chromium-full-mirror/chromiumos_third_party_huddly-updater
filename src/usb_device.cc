// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>

#include "tools.h"
#include "usb_device.h"

namespace huddly {

const int kStrBufSize = 200;

UsbDevice::UsbDevice()
    : vendor_id_(0),
      product_id_(0),
      interface_number_(0),
      was_kernel_driver_active_(false),
      is_claimed_(false),
      context_(nullptr),
      dev_(nullptr),
      dev_handle_(nullptr) {}

UsbDevice::UsbDevice(uint16_t vendor_id, uint16_t product_id)
    : vendor_id_(vendor_id),
      product_id_(product_id),
      interface_number_(0),
      was_kernel_driver_active_(false),
      is_claimed_(false),
      context_(nullptr),
      dev_(nullptr),
      dev_handle_(nullptr) {}

UsbDevice::~UsbDevice() {
  std::string err_msg;
  if (!Teardown(&err_msg)) {
    std::cout << ".. error in teardown: " << err_msg << std::endl;
  }
}

bool UsbDevice::Setup(std::string* err_msg) {
  // Set context
  if (libusb_init(&context_)) {
    *err_msg += ".. failed to initialize libusb_context";
    return false;
  }

  libusb_set_debug(context_, LIBUSB_LOG_LEVEL_INFO);

  if (!GetDevHandle(err_msg)) {
    *err_msg += ".. failed to get device handle for " + GetId();
    return false;
  }

  // Claim the interface
  interface_number_ = GetInterfaceNumber();
  is_claimed_ = ClaimInterface(err_msg);
  if (!is_claimed_) {
    *err_msg += ".. failed to claim the interface number " +
                std::to_string(interface_number_);
    return false;
  }

  return true;
}

bool UsbDevice::Setup(uint16_t vendor_id,
                      uint16_t product_id,
                      std::string* err_msg) {
  vendor_id_ = vendor_id;
  product_id_ = product_id;
  return Setup(err_msg);
}

bool UsbDevice::Teardown(std::string* err_msg) {
  // Multiple steps in tear down process.
  // Even if one step fails, the next step continues.
  // Error messages are appended.

  bool is_success = ReleaseInterface(err_msg);

  if (dev_handle_) {
    libusb_close(dev_handle_);
  }
  if (context_) {
    libusb_exit(context_);
  }

  return is_success;
}

bool UsbDevice::ControlTransfer(uint8_t request_type,
                                uint8_t request,
                                uint32_t address,
                                uint32_t data_len,
                                uint8_t* data,
                                std::string* err_msg) const {
  uint8_t libusb_request_type = static_cast<uint8_t>(request_type);
  uint8_t libusb_request = request & 0xff;
  uint16_t libusb_value = address & 0xffff;
  uint16_t libusb_index = (address >> 16) & 0xffff;

  const int kUsbReadWriteTimeOut = 1000;  // msec

  /*  Debug line. Keep this till code freeze
      TODO(porce): Delete by May 12 2017
    printf(
        "DBG: LIBUSB_CONTROL_TRANSFER: devhandle %p  reqtype %02x req %02x value
    "
        "%02x "
        "idx "
        "%02x ::  datalen %u data_first_char %c\n",
        dev_handle_, libusb_request_type, libusb_request, libusb_value,
        libusb_index, data_len, data[0]);
  */
  int ret = libusb_control_transfer(dev_handle_, libusb_request_type,
                                    libusb_request, libusb_value, libusb_index,
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
      data_len, ret, libusb_request_type, libusb_request, libusb_value,
      libusb_index);
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

bool UsbDevice::GetDevHandle(std::string* err_msg) {
  // Get device list
  // TODO(porce): Use unique_ptr and custom deleter
  libusb_device** dev_list;
  int list_cnt = libusb_get_device_list(context_, &dev_list);
  if (list_cnt < 0) {
    *err_msg += ".. failed to get device list: " + ErrCodeToErrMsg(list_cnt);
    return false;
  }

  if (list_cnt == 0) {
    *err_msg += ".. no usb device is found";
    libusb_free_device_list(dev_list, 1);  // unref by 1
    return false;
  }

  libusb_device* dev;
  libusb_device_descriptor desc;
  bool is_found = false;
  for (int idx = 0; idx < list_cnt; idx++) {
    dev = dev_list[idx];
    int ret = libusb_get_device_descriptor(dev, &desc);
    if (ret != 0) {
      std::cout << ".. failed to get device descriptor: "
                << ErrCodeToErrMsg(ret) << std::endl;
      continue;
    }
    if (desc.idVendor == vendor_id_ && desc.idProduct == product_id_) {
      // Supported use case: one or zero camera connected to a single box.
      // TODO(porce): Extend this upon use case change.
      is_found = true;
      break;
    }
  }

  if (!is_found) {
    *err_msg +=
        ".. failed to get device handle. no device found with " + GetId();
    libusb_free_device_list(dev_list, 1);  // unref by 1
    return false;
  }

  int ret = libusb_open(dev, &dev_handle_);
  if (ret != 0) {
    *err_msg += ".. failed to get device handle: " + ErrCodeToErrMsg(ret);
    libusb_free_device_list(dev_list, 1);  // unref by 1
    return false;
  }

  dev_ = dev;
  libusb_ref_device(dev);
  libusb_free_device_list(dev_list, 1);  // unref by 1
  return true;
}

bool UsbDevice::ClaimInterface(std::string* err_msg) {
  int ret = libusb_kernel_driver_active(dev_handle_, interface_number_);
  if (ret == 0) {
    was_kernel_driver_active_ = false;
    // No need to detach.
  } else if (ret < 0) {
    *err_msg += ".. failed to check if the kernel driver is active: " +
                ErrCodeToErrMsg(ret);
    return false;
  } else if (ret == 1) {
    // A kernel driver is active.
    was_kernel_driver_active_ = true;
    if ((ret = libusb_detach_kernel_driver(dev_handle_, interface_number_)) !=
        0) {
      *err_msg +=
          ".. failed to detach active kernel driver: " + ErrCodeToErrMsg(ret);
      return false;
    }
  }

  // Now the kernel driver is detached.

  /*  TODO(porce): Investigate the need of configuring Huddly camera
    int configuration = 1;  // bConfigurationValue
    if ((ret = libusb_set_configuration(dev_handle_, configuration)) != 0) {
      *err_msg = ErrCodeToErrMsg(ret);
      return false;
    }
    // Now configured.
  */

  if ((ret = libusb_claim_interface(dev_handle_, interface_number_)) != 0) {
    *err_msg += ".. failed to claim: " + ErrCodeToErrMsg(ret);
    return false;
  }

  // Now device handle's interface is claimed.
  return true;
}

bool UsbDevice::ReleaseInterface(std::string* err_msg) {
  bool is_success = true;

  if (is_claimed_) {
    int ret = libusb_release_interface(dev_handle_, interface_number_);
    if (ret != 0) {
      *err_msg += ".. failed to release: " + ErrCodeToErrMsg(ret);
      is_success = false;
    }
  }
  is_claimed_ = false;

  if (was_kernel_driver_active_) {
    int ret = libusb_attach_kernel_driver(dev_handle_, interface_number_);
    if (ret != 0) {
      *err_msg += ".. failed to reattach detached kernel driver: " +
                  ErrCodeToErrMsg(ret);
      is_success = false;
    }
  }
  was_kernel_driver_active_ = false;

  return is_success;
}

int UsbDevice::GetInterfaceNumber() const {
  // TODO(porce): put some proper logic..
  return 0;
}

}  // namespace huddly
