// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "hlink_buffer.h"

#include <cassert>

namespace huddly {
HLinkBuffer::HLinkBuffer(const std::string& msg_name,
                         const uint8_t* payload,
                         const int payload_size)
    : msg_name_(msg_name), payload_(payload, payload + payload_size) {
  header_.req_id = 0;
  header_.res_id = 0;
  header_.flags = 0;
  header_.msg_name_size = static_cast<uint16_t>(msg_name.size());
  header_.payload_size = payload_size;
}

bool HLinkBuffer::FromRawBuffer(const std::vector<uint8_t>& raw_buffer,
                                HLinkBuffer* hlink_buffer,
                                std::string* err_msg) {
  const auto header = *reinterpret_cast<const HLinkHeader*>(&raw_buffer[0]);
  if (raw_buffer.size() != header.TotalSize()) {
    *err_msg =
        "HLink Buffer mismatch between header size field and actual size.";
    return false;
  }

  hlink_buffer->header_ = header;
  const auto msg_name_begin =
      reinterpret_cast<const char*>(&raw_buffer[0] + sizeof(header));
  const auto msg_name_end = msg_name_begin + header.msg_name_size;
  hlink_buffer->msg_name_ = std::string(msg_name_begin, msg_name_end);

  const auto payload_begin = raw_buffer.cbegin() + header.HeaderSize();
  const auto payload_end = payload_begin + header.payload_size;
  hlink_buffer->payload_ = std::vector<uint8_t>(payload_begin, payload_end);
  return true;
}

std::string HLinkBuffer::GetMessageName() const {
  return msg_name_;
}

const std::vector<uint8_t>& HLinkBuffer::GetPayload() const {
  return payload_;
}

std::vector<uint8_t> HLinkBuffer::CreatePacket() const {
  std::vector<uint8_t> data;
  for (auto i = 0; i < sizeof(header_); i++) {
    data.push_back(reinterpret_cast<const uint8_t*>(&header_)[i]);
  }
  data.insert(std::end(data), std::begin(msg_name_), std::end(msg_name_));
  data.insert(std::end(data), std::begin(payload_), std::end(payload_));
  return data;
}

}  // namespace huddly
