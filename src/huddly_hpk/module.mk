# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

PC_DEPS = libbrillo-$(BASE_VER) libchrome-$(BASE_VER) libusb-1.0 libudev msgpack
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))
LDLIBS += $(PC_LIBS)

CPPFLAGS += $(PC_CFLAGS)

UNITTEST_LIBS := $(shell gtest-config --libs)

CXX_BINARY(huddly-hpk-unittest): LDLIBS += $(UNITTEST_LIBS)
CXX_BINARY(huddly-hpk-unittest): \
	src/huddly_hpk/hpk_file.o \
	src/huddly_hpk/hpk_file_unittest.o \
	src/huddly_go/testrunner.o

tests: TEST(CXX_BINARY(huddly-hpk-unittest))
clean: CLEAN(CXX_BINARY(huddly-hpk-unittest))
