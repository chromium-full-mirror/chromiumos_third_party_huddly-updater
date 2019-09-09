// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "message_bus.h"

namespace huddly {
namespace message_bus {

bool Subscribe(HLinkVsc* hlink, const std::string& subscription) {
  return hlink->Send("hlink-mb-subscribe",
                     reinterpret_cast<const uint8_t*>(&subscription[0]),
                     subscription.size());
}

bool Unsubscribe(HLinkVsc* hlink, const std::string& subscription) {
  return hlink->Send("hlink-mb-unsubscribe",
                     reinterpret_cast<const uint8_t*>(&subscription[0]),
                     subscription.size());
}

std::unique_ptr<ScopedSubscribe> ScopedSubscribe::Create(
    HLinkVsc* hlink, const std::string& subscription) {
  auto instance = std::unique_ptr<ScopedSubscribe>(
      new ScopedSubscribe(hlink, subscription));
  if (!Subscribe(hlink, subscription)) {
    return nullptr;
  }
  return instance;
}

ScopedSubscribe::~ScopedSubscribe() {
  Unsubscribe(hlink_, subscription_);
}

ScopedSubscribe::ScopedSubscribe(HLinkVsc* hlink,
                                 const std::string& subscription)
    : hlink_(hlink), subscription_(subscription) {}

}  // namespace message_bus
}  // namespace huddly
