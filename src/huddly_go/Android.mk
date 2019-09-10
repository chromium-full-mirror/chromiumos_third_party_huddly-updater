# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libhuddly-go-updater
LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true

LOCAL_SRC_FILES := \
    tools.cc \
    manifest.cc \
    usb_device.cc \
    minicam_device.cc \
    firmware.cc \
    flasher.cc

LOCAL_C_INCLUDES := \
  $(LOCAL_PATH) \
  external/libusb/libusb

LOCAL_SHARED_LIBRARIES += \
    libchrome \
    libusb1.0

# ZipArchive support, the order matters here to get all symbols.
LOCAL_STATIC_LIBRARIES := libziparchive libz libbase
# For android::FileMap used by libziparchive.
LOCAL_SHARED_LIBRARIES += libutils
# For liblog, atrace, properties, ashmem, set_sched_policy
# and socket_peer_is_trusted.
LOCAL_SHARED_LIBRARIES += libcutils

LOCAL_CFLAGS := -Wall -Werror -Wno-sign-promo -Wno-unused-parameter

include $(BUILD_SHARED_LIBRARY)
