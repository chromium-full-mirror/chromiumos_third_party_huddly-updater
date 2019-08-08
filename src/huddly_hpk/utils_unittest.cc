// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils.h"
#include <vector>

#include <gtest/gtest.h>

namespace {

typedef std::vector<uint32_t> version;

class UtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(UtilsTest, VersionCompareEqualVersionsReturnsZero) {
  EXPECT_EQ(0, huddly::VersionCompare(version{1, 2, 3}, version{1, 2, 3}));
  EXPECT_EQ(0, huddly::VersionCompare(version{1000, 2000, 3000},
                                      version{1000, 2000, 3000}));
}

TEST_F(UtilsTest, VersionCompareLowerFirstVersionsReturnsNegative) {
  EXPECT_EQ(-1, huddly::VersionCompare(version{1, 0, 0}, version{1, 0, 1}));
  EXPECT_EQ(-1, huddly::VersionCompare(version{1, 0, 0}, version{1, 1, 0}));
  EXPECT_EQ(-1, huddly::VersionCompare(version{1, 0, 0}, version{2, 0, 0}));
  EXPECT_EQ(-1, huddly::VersionCompare(version{500, 2000, 20},
                                       version{500, 2000, 21}));
  EXPECT_EQ(-1, huddly::VersionCompare(version{500, 2000, 20},
                                       version{500, 2001, 20}));
  EXPECT_EQ(-1, huddly::VersionCompare(version{500, 2000, 20},
                                       version{501, 2000, 20}));
}

TEST_F(UtilsTest, VersionCompareHigherFirstVersionsReturnsPositive) {
  EXPECT_EQ(1, huddly::VersionCompare(version{1, 0, 1}, version{1, 0, 0}));
  EXPECT_EQ(1, huddly::VersionCompare(version{1, 1, 0}, version{1, 0, 0}));
  EXPECT_EQ(1, huddly::VersionCompare(version{2, 0, 0}, version{1, 0, 0}));
  EXPECT_EQ(1, huddly::VersionCompare(version{500, 2000, 21},
                                      version{500, 2000, 20}));
  EXPECT_EQ(1, huddly::VersionCompare(version{500, 2001, 20},
                                      version{500, 2000, 20}));
  EXPECT_EQ(1, huddly::VersionCompare(version{501, 2000, 20},
                                      version{500, 2000, 20}));
}

}  // namespace
