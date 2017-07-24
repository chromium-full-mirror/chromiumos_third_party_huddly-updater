# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

PC_DEPS = libbrillo-$(BASE_VER) libchrome-$(BASE_VER) libusb-1.0 libudev
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))
LDLIBS += $(PC_LIBS)

CPPFLAGS += $(PC_CFLAGS)

CXX_BINARY(huddly-updater): \
	src/firmware.o \
	src/flasher.o \
	src/main.o \
	src/minicam_device.o \
	src/tools.o \
	src/usb_device.o

CXX_BINARY(huddly-manifest): \
	src/manifest.o \
	src/manifest_main.o

all: CXX_BINARY(huddly-updater) CXX_BINARY(huddly-manifest)
clean: CLEAN(CXX_BINARY(huddly-updater)) CLEAN(CXX_BINARY(huddly-manifest))

UNITTEST_LIBS := $(shell gtest-config --libs)

CXX_BINARY(manifest-unittest): LDLIBS += $(UNITTEST_LIBS)
CXX_BINARY(manifest-unittest): \
	src/manifest.o \
	src/manifest_unittest.o \
	src/testrunner.o

tests: TEST(CXX_BINARY(manifest-unittest))
clean: CLEAN(CXX_BINARY(manifest-unittest))
