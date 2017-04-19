// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_TOOLS_H_
#define SRC_TOOLS_H_

#include <string>

namespace huddly {

bool RunCommand(const std::string &cmd, std::string *output);

}  // namespace huddly

#endif  // SRC_TOOLS_H_
