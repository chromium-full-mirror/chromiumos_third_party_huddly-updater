// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SRC_HUDDLY_HPK_HCP_H_
#define SRC_HUDDLY_HPK_HCP_H_

#include <cstdint>
#include <string>

#include "hlink_vsc.h"

namespace huddly {
namespace hcp {

bool Write(HLinkVsc* hlink,
           const std::string& filename,
           const std::string& data);
bool Write(HLinkVsc* hlink,
           const std::string& filename,
           const uint8_t* data,
           size_t sz);

}  // namespace hcp
}  // namespace huddly

#endif  // SRC_HUDDLY_HPK_HCP_H_
