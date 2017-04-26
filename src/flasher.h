// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_FLASHER_H_
#define SRC_FLASHER_H_

#include <string>

#include "firmware.h"
#include "minicam_device.h"

namespace huddly {

// class Flasher stands on top of a Firmware object and a MinicamDevice object.
// This flasher shall run in a single threaded process.
// It is illegal to delete the Firmware or Minicam object unless Flasher
// completes it job.
class Flasher {
 public:
  Flasher(const Firmware& firmware, MinicamDevice* minidev);
  ~Flasher();
  bool FlashApp(std::string* err_msg) const;

 private:
  Flasher(const Flasher&) = delete;
  Flasher& operator=(const Flasher&) = delete;

  const Firmware& firmware_;
  MinicamDevice* minidev_;
};

}  // namespace huddly

#endif  // SRC_FLASHER_H_
