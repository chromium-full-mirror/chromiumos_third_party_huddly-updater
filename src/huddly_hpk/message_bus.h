// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SRC_HUDDLY_HPK_MESSAGE_BUS_H_
#define SRC_HUDDLY_HPK_MESSAGE_BUS_H_

#include <base/macros.h>
#include <memory>
#include <string>
#include "hlink_vsc.h"

namespace huddly {
namespace message_bus {

bool Subscribe(HLinkVsc* hlink, const std::string& subscription);
bool Unsubscribe(HLinkVsc* hlink, const std::string& subscription);

// Use ScopedSubscribe to automatically unsubscribe when the ScopedSubscribe
// object goes out of scope. Should be preferred in most cases over calling
// Unsubscribe directly.
class ScopedSubscribe {
public:
  static std::unique_ptr<ScopedSubscribe> Create(
      HLinkVsc* hlink, const std::string& subscription);
  ~ScopedSubscribe();

private:
  ScopedSubscribe(HLinkVsc* hlink, const std::string& subscription);
  HLinkVsc* hlink_ = nullptr;
  std::string subscription_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSubscribe);
};

}  // namespace message_bus
}  // namespace huddly

#endif  // SRC_HUDDLY_HPK_MESSAGE_BUS_H_
