# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

PC_DEPS = libbrillo libchrome libusb-1.0 libudev msgpack-c
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))
LDLIBS += $(PC_LIBS)

CPPFLAGS += $(PC_CFLAGS)

UNITTEST_LIBS := -lgtest -pthread -lpthread

CXX_BINARY(messagepack-unittest): LDLIBS += $(UNITTEST_LIBS)
CXX_BINARY(messagepack-unittest): \
	src/common/messagepack/messagepack.o \
	src/common/messagepack/messagepack_unittest.o \
	src/common/messagepack/testrunner.o

tests: TEST(CXX_BINARY(messagepack-unittest))
clean: CLEAN(CXX_BINARY(messagepack-unittest))
