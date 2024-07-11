# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

PC_DEPS = libbrillo libchrome libusb-1.0 libudev cfm-dfu-notification
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))
LDLIBS += $(PC_LIBS)

CPPFLAGS += $(PC_CFLAGS)

CXX_BINARY(huddly-updater): \
	src/huddly_go/firmware.o \
	src/huddly_go/flasher.o \
	src/huddly_go/main.o \
	src/huddly_go/manifest.o \
	src/huddly_go/minicam_device.o \
	src/huddly_go/tools.o \
	src/huddly_go/usb_device.o

all: CXX_BINARY(huddly-updater)
clean: CLEAN(CXX_BINARY(huddly-updater))

UNITTEST_LIBS := -lgtest -pthread -lpthread

CXX_BINARY(manifest-unittest): LDLIBS += $(UNITTEST_LIBS)
CXX_BINARY(manifest-unittest): \
	src/huddly_go/manifest.o \
	src/huddly_go/manifest_unittest.o \
	src/huddly_go/testrunner.o

tests: TEST(CXX_BINARY(manifest-unittest))
clean: CLEAN(CXX_BINARY(manifest-unittest))
