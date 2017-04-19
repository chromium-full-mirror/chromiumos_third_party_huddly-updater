# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CXX_BINARY(huddly-updater): src/main.o src/firmware.o src/tools.o
all: CXX_BINARY(huddly-updater)
clean: CLEAN(CXX_BINARY(huddly-updater))
