# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libhuddly-hpk-updater
LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true

LOCAL_SRC_FILES := \
	hcp.cc \
	hlink_buffer.cc \
	hlink_vsc.cc \
	hpk_file.cc \
	hpk_updater.cc \
	message_bus.cc \
	usb.cc \
	utils.cc

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) \
	external/libusb/libusb \
	external/msgpack-c/include

LOCAL_SHARED_LIBRARIES := \
	libhuddly-common-msgpack \
	libbrillo \
	libchrome \
	libusb1.0 \
	libmsgpack

LOCAL_CFLAGS := \
	-Wall -Werror -fexceptions \
	-Wno-tautological-compare \
	-Wno-sign-compare -Wno-unused-parameter

include $(BUILD_SHARED_LIBRARY)
