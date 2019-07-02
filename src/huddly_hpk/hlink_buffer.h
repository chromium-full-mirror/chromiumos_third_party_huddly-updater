// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SRC_HUDDLY_HPK_HLINK_BUFFER_H_
#define SRC_HUDDLY_HPK_HLINK_BUFFER_H_

#include <cstdint>
#include <string>
#include <vector>

namespace huddly {

struct __attribute__((packed)) HLinkHeader {
  uint32_t req_id = 0;
  uint32_t res_id = 0;
  uint16_t flags = 0;
  uint16_t msg_name_size = 0;
  uint32_t payload_size = 0;

  size_t HeaderSize() const { return sizeof(HLinkHeader) + msg_name_size; }
  size_t TotalSize() const {
    return sizeof(HLinkHeader) + msg_name_size + payload_size;
  }
};
static_assert(sizeof(HLinkHeader) == 16, "HLinkHeader should be 16 bytes");

class HLinkBuffer {
 public:
  HLinkBuffer() {}
  HLinkBuffer(const std::string& msg_name,
              const uint8_t* payload,
              const int payload_size);
  static bool FromRawBuffer(const std::vector<uint8_t>& raw_buffer,
                            HLinkBuffer* hlink_buffer,
                            std::string* err_msg);

  std::string GetMessageName() const;
  const std::vector<uint8_t>& GetPayload() const;
  std::vector<uint8_t> CreatePacket() const;

 private:
  HLinkHeader header_;
  std::string msg_name_;
  std::vector<uint8_t> payload_;
};

}  // namespace huddly

#endif  // SRC_HUDDLY_HPK_HLINK_BUFFER_H_
