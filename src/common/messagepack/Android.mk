# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libhuddly-common-msgpack
LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true

LOCAL_SRC_FILES := messagepack.cc

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) \
	external/msgpack-c/include

LOCAL_SHARED_LIBRARIES := \
	libchrome \
	libmsgpack

LOCAL_CFLAGS := \
	-Wall -Werror -Wno-unused-parameter \
	-Wno-sign-compare -Wunused -Wunreachable-code

include $(BUILD_SHARED_LIBRARY)

