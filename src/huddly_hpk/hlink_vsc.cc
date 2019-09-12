// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "hlink_vsc.h"

#include <base/logging.h>
#include <msgpack.h>
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <msgpack.hpp>
#include "../common/messagepack/messagepack.h"
#include "message_bus.h"
#include "utils.h"

namespace huddly {

constexpr uint8_t kVendorSpecificClass = 0xff;
constexpr uint8_t kEndpointDirectionMask = 0x80;
constexpr auto kUsbWaitDetachTimeoutMillisec = 10000;
constexpr auto kUsbBulkTransferTimeoutMillisec = 10000;

std::unique_ptr<HLinkVsc> HLinkVsc::Create(
    std::unique_ptr<UsbDevice> usb_device) {
  return std::unique_ptr<HLinkVsc>(new HLinkVsc(std::move(usb_device)));
}

HLinkVsc::HLinkVsc(std::unique_ptr<UsbDevice> usb_device)
    : usb_device_(std::move(usb_device)) {}

HLinkVsc::~HLinkVsc() {
  if (interface_claimed_) {
    ReleaseInterface();
  }
}

UsbDevice* HLinkVsc::GetUsbDevice() {
  return usb_device_.get();
}

bool HLinkVsc::Connect() {
  if (!FindInterface()) {
    LOG(ERROR) << "HLink failed to find HLink interface";
    return false;
  }
  if (!ClaimInterface()) {
    LOG(ERROR) << "HLink failed to claim interface";
    return false;
  }
  if (!SendReset()) {
    LOG(ERROR) << "HLink send reset failed";
    return false;
  }
  if (!Salute()) {
    LOG(ERROR) << "HLink salute failed";
    return false;
  }
  return true;
}

bool HLinkVsc::Send(const HLinkBuffer& hlbuf) {
  constexpr int kMaxChunkSize = 16 * 1024;
  auto packet = hlbuf.CreatePacket();

  int left_to_transfer = packet.size();
  int total_transferred = 0;
  while (left_to_transfer) {
    int chunk_sz = std::min(left_to_transfer, kMaxChunkSize);
    auto chunk_begin = packet.cbegin() + total_transferred;
    auto chunk_end = chunk_begin + chunk_sz;
    std::vector<uint8_t> chunk(chunk_begin, chunk_end);
    if (!usb_device_->BulkWrite(endpoint_out_, chunk,
                                kUsbBulkTransferTimeoutMillisec)) {
      LOG(ERROR) << "Failed to send buffer";
      return false;
    }
    left_to_transfer -= chunk_sz;
    total_transferred += chunk_sz;
  }
  return true;
}

bool HLinkVsc::Send(std::string msg_name, const uint8_t* data, int sz) {
  HLinkBuffer hlbuf(msg_name.c_str(), data, sz);
  return Send(hlbuf);
}

bool HLinkVsc::Receive(HLinkBuffer* hlink_buffer) {
  std::vector<uint8_t> packet;
  if (!ReceivePacket(&packet)) {
    LOG(ERROR) << "Failed to receive header";
    return false;
  }

  if (packet.size() < sizeof(HLinkHeader)) {
    LOG(ERROR) << "Expected HLinkHeader, but only got " << packet.size()
               << " bytes.";
    return false;
  }
  HLinkHeader header = *reinterpret_cast<HLinkHeader*>(packet.data());

  while (packet.size() < header.TotalSize()) {
    std::vector<uint8_t> fragment;
    if (!ReceivePacket(&fragment)) {
      LOG(ERROR) << "Failed to receive packet";
      return false;
    }

    packet.insert(std::end(packet), std::begin(fragment), std::end(fragment));
  }

  std::string err_msg;
  if (!HLinkBuffer::FromRawBuffer(packet, hlink_buffer, &err_msg)) {
    LOG(ERROR) << err_msg;
    return false;
  }
  return true;
}

bool HLinkVsc::SendReboot(bool wait_for_detach) {
  interface_claimed_ = false;  // Avoid libusb error due to device reboot.
  if (!Send("camctrl/reboot", nullptr, 0)) {
    LOG(ERROR) << "Failed to send reboot command";
  }

  if (!wait_for_detach) {
    return true;
  }

  LOG(INFO) << "Waiting for the device to detach...";

  if (!usb_device_->WaitForDetach(kUsbWaitDetachTimeoutMillisec)) {
    LOG(ERROR) << "Device detach failed";
    return false;
  }
  LOG(INFO) << "Device detached";
  return true;
}

bool HLinkVsc::FindInterface() {
  auto config = usb_device_->GetActiveConfigDescriptor();
  if (!config) {
    LOG(ERROR) << "Failed to get active configuration descriptor";
    return false;
  }

  const int num_interfaces = config->bNumInterfaces;

  // Find interface descriptor for the interface with USB class == VSC.
  const auto interface_begin = config->interface;
  const auto interface_end = config->interface + num_interfaces;
  const auto is_vendor_specific_class = [](const libusb_interface interface) {
    return interface.altsetting[0].bInterfaceClass == kVendorSpecificClass;
  };
  const auto hlink_interface =
      std::find_if(interface_begin, interface_end, is_vendor_specific_class);
  if (hlink_interface == interface_end) {
    LOG(ERROR) << "Could not find HLink interface. Number of interfaces: "
               << num_interfaces;
    return false;
  }

  if (hlink_interface->num_altsetting != 1) {
    LOG(WARNING) << "Unexpected number of altsettings";
  }

  const auto hlink_interface_descr = &hlink_interface->altsetting[0];
  interface_number_ = hlink_interface_descr->bInterfaceNumber;
  VLOG(1) << "Interface " << static_cast<int>(interface_number_)
          << " is VSC interface. Using this interface for HLink.";

  // Find input and output endpoints.
  const auto endpoint_begin = hlink_interface_descr->endpoint;
  const auto endpoint_end =
      endpoint_begin + hlink_interface_descr->bNumEndpoints;

  const auto is_out_endpoint = [](const libusb_endpoint_descriptor& endpoint) {
    return (endpoint.bEndpointAddress & kEndpointDirectionMask) == 0;
  };
  const auto endpoint_out_it =
      std::find_if(endpoint_begin, endpoint_end, is_out_endpoint);
  if (endpoint_out_it == endpoint_end) {
    LOG(ERROR) << "No out endpoint found";
    return false;
  }

  const auto is_in_endpoint = [](const libusb_endpoint_descriptor& endpoint) {
    return (endpoint.bEndpointAddress & kEndpointDirectionMask) != 0;
  };
  const auto endpoint_in_it =
      std::find_if(endpoint_begin, endpoint_end, is_in_endpoint);
  if (endpoint_in_it == endpoint_end) {
    LOG(ERROR) << "No in endpoint found";
    return false;
  }

  endpoint_out_ = UsbEndpoint(*endpoint_out_it);
  endpoint_in_ = UsbEndpoint(*endpoint_in_it);
  VLOG(1) << " endpoint out: 0x" << Uint8ToHexString(endpoint_out_.address())
          << " endpoint in: 0x" << Uint8ToHexString(endpoint_in_.address());

  return true;
}

bool HLinkVsc::ClaimInterface() {
  if (!usb_device_->ClaimInterface(interface_number_)) {
    LOG(ERROR) << "Failed to claim interface " << interface_number_;
    return false;
  }
  interface_claimed_ = true;
  return true;
}

bool HLinkVsc::ReleaseInterface() {
  if (!interface_claimed_) {
    LOG(WARNING) << "Interface not claimed: " << interface_number_;
    return false;
  }

  if (!usb_device_->ReleaseInterface(interface_number_)) {
    LOG(WARNING) << "Failed to release interface " << interface_number_;
    return false;
  }
  interface_claimed_ = false;
  return true;
}

bool HLinkVsc::SendReset() {
  const std::vector<uint8_t> kResetSeq = {};  // Empty packet.

  // Send reset twice. This is a workaround that resolves an issue that occurs
  // if the last transfer was interrupted, for example because the user pressed
  // ctrl+c. In this case, the reset packet would be lost.
  for (auto i = 0; i < 2; i++) {
    if (!usb_device_->BulkWrite(endpoint_out_, kResetSeq,
                                kUsbBulkTransferTimeoutMillisec)) {
      LOG(ERROR) << "Failed to send reset sequence.";
      return false;
    }
  }
  return true;
}

bool HLinkVsc::Salute() {
  std::vector<uint8_t> kSalutation = {0};
  if (!usb_device_->BulkWrite(endpoint_out_, kSalutation,
                              kUsbBulkTransferTimeoutMillisec)) {
    LOG(ERROR) << "Failed to send salutation.";
    return false;
  }

  std::vector<uint8_t> salutation_reply;
  if (!usb_device_->BulkRead(endpoint_in_, kUsbBulkTransferTimeoutMillisec,
                             &salutation_reply)) {
    LOG(ERROR) << "Failed to read salutation.";
    return false;
  }

  const std::vector<uint8_t> kExpectedSalutationReply{'H', 'L', 'i', 'n',
                                                      'k', ' ', 'v', '0'};
  if (salutation_reply != kExpectedSalutationReply) {
    LOG(ERROR) << "Unexpected salutation received.";
    return false;
  }
  return true;
}

bool HLinkVsc::ReceivePacket(std::vector<uint8_t>* packet) {
  if (!usb_device_->BulkRead(endpoint_in_, kUsbBulkTransferTimeoutMillisec,
                             packet)) {
    LOG(ERROR) << "Bulk read failed.";
    return false;
  }
  return true;
}

}  // namespace huddly
