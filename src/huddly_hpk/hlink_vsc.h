// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SRC_HUDDLY_HPK_HLINK_VSC_H_
#define SRC_HUDDLY_HPK_HLINK_VSC_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "hlink_buffer.h"
#include "usb.h"

namespace huddly {

class HLinkVsc {
 public:
  static std::unique_ptr<HLinkVsc> Create(
      std::unique_ptr<UsbDevice> usb_device);

  HLinkVsc(const HLinkVsc&) = delete;
  HLinkVsc& operator=(const HLinkVsc&) = delete;

  ~HLinkVsc();
  UsbDevice* GetUsbDevice();
  bool Connect();
  bool Send(const HLinkBuffer& hlbuf);
  bool Send(std::string msg_name, const uint8_t* data, int sz);
  bool Receive(HLinkBuffer* buffer);
  bool SendReboot(bool wait_for_detach);

 private:
  explicit HLinkVsc(std::unique_ptr<UsbDevice> usb_device);
  HLinkVsc() {}

  bool FindInterface();
  bool ClaimInterface();
  bool ReleaseInterface();
  bool SendReset();
  bool Salute();
  bool ReceivePacket(std::vector<uint8_t>* packet);

  std::unique_ptr<UsbDevice> usb_device_;
  int interface_number_ = -1;
  bool interface_claimed_ = false;
  UsbEndpoint endpoint_in_;
  UsbEndpoint endpoint_out_;
};

}  // namespace huddly

#endif  // SRC_HUDDLY_HPK_HLINK_VSC_H_
