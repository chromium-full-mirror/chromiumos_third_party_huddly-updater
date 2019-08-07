// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hcp.h"

#include <base/logging.h>
#include <msgpack.h>

#include "../common/messagepack/messagepack.h"
#include "message_bus.h"

namespace huddly {
namespace hcp {

bool Write(HLinkVsc* hlink,
           const std::string& filename,
           const std::string& data) {
  return Write(hlink, filename, reinterpret_cast<const uint8_t*>(data.data()),
               data.size());
}

bool Write(HLinkVsc* hlink,
           const std::string& filename,
           const uint8_t* data,
           size_t sz) {
  msgpack_sbuffer sbuf;
  msgpack_sbuffer_init(&sbuf);

  msgpack_packer pk;
  msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);
  msgpack_pack_map(&pk, 2);
  msgpack_pack_str(&pk, 4);
  msgpack_pack_str_body(&pk, "name", 4);
  msgpack_pack_str(&pk, filename.size());
  msgpack_pack_str_body(&pk, filename.c_str(), filename.size());
  msgpack_pack_str(&pk, 9);
  msgpack_pack_str_body(&pk, "file_data", 9);
  msgpack_pack_bin(&pk, sz);
  msgpack_pack_bin_body(&pk, reinterpret_cast<const char*>(data), sz);

  const std::string kSubscription("hcp/write_reply");
  const auto scoped_subscription =
      message_bus::ScopedSubscribe::Create(hlink, kSubscription);
  if (!scoped_subscription) {
    LOG(ERROR) << "Failed to subscribe to " << kSubscription;
    return false;
  }

  if (!hlink->Send("hcp/write", reinterpret_cast<uint8_t*>(sbuf.data),
                   sbuf.size)) {
    LOG(ERROR) << "Failed to send hcp/write command";
    return false;
  }

  huddly::HLinkBuffer hl_buffer;
  if (!hlink->Receive(&hl_buffer)) {
    LOG(ERROR) << "Failed to receive packet";
    return false;
  }

  auto unpacker = messagepack::Unpacker::Create(hl_buffer.GetPayload());
  if (!unpacker) {
    LOG(ERROR) << "Failed to decode 'hcp/write_reply' message payload";
    return false;
  }
  messagepack::Map hcp_write_reply;
  if (!unpacker->GetRoot<messagepack::Map>(&hcp_write_reply)) {
    LOG(ERROR) << "Failed to get '" << kSubscription << "' as map";
    return false;
  }
  int64_t status;
  if (!hcp_write_reply.GetValueAs<int64_t>("status", &status)) {
    LOG(ERROR) << "Failed to get status field from '"
               << hl_buffer.GetMessageName() << "' message";
    return false;
  }

  if (status) {
    LOG(ERROR) << "'" << hl_buffer.GetMessageName()
               << "' status field is non-zero: " << status;
    return false;
  }

  return true;
}
}  // namespace hcp
}  // namespace huddly
