# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

PC_DEPS = libchrome-$(BASE_VER) libusb-1.0 libudev
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))
LDLIBS += $(PC_LIBS)

CPPFLAGS += $(PC_CFLAGS)

CXX_BINARY(huddly-updater): src/main.o src/firmware.o src/tools.o src/usb_device.o src/flasher.o src/minicam_device.o
all: CXX_BINARY(huddly-updater)
clean: CLEAN(CXX_BINARY(huddly-updater))
