# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

PC_DEPS = libbrillo libchrome libusb-1.0 libudev msgpack-c cfm-dfu-notification
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))
LDLIBS += $(PC_LIBS)

CPPFLAGS += $(PC_CFLAGS)

CXX_BINARY(huddly-hpk-updater): \
	src/common/messagepack/messagepack.o \
	src/huddly_hpk/hcp.o \
	src/huddly_hpk/hlink_buffer.o \
	src/huddly_hpk/hlink_vsc.o \
	src/huddly_hpk/hpk_file.o \
	src/huddly_hpk/hpk_updater.o \
	src/huddly_hpk/main.o \
	src/huddly_hpk/message_bus.o \
	src/huddly_hpk/usb.o \
	src/huddly_hpk/utils.o

all: CXX_BINARY(huddly-hpk-updater)
clean: CLEAN(CXX_BINARY(huddly-hpk-updater))

UNITTEST_LIBS := -lgtest -pthread -lpthread

CXX_BINARY(huddly-hpk-unittest): LDLIBS += $(UNITTEST_LIBS)
CXX_BINARY(huddly-hpk-unittest): \
	src/huddly_hpk/hpk_file.o \
	src/huddly_hpk/hpk_file_unittest.o \
	src/huddly_hpk/testrunner.o \
	src/huddly_hpk/utils.o \
	src/huddly_hpk/utils_unittest.o

tests: TEST(CXX_BINARY(huddly-hpk-unittest))
clean: CLEAN(CXX_BINARY(huddly-hpk-unittest))
