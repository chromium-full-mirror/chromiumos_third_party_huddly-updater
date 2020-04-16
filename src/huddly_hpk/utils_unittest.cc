// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils.h"
#include <vector>

#include <gtest/gtest.h>

namespace {

using huddly::NumericVersion;
using huddly::StripAnsiSgrCodes;

class UtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(UtilsTest, NumericVersionDefaultCtor) {
  EXPECT_EQ(NumericVersion{}, NumericVersion({0, 0, 0}));
}

TEST_F(UtilsTest, NumericVersionAssignmentCtor) {
  const NumericVersion a({1, 2, 3});
  const NumericVersion b = a;
  EXPECT_EQ(b, a);
}

TEST_F(UtilsTest, NumericVersionCopyCtor) {
  const NumericVersion a({4, 5, 6});
  const NumericVersion b(a);
  EXPECT_EQ(b, a);
}

TEST_F(UtilsTest, NumericVersionOperatorEquals) {
  EXPECT_EQ(NumericVersion({1, 2, 3}), NumericVersion({1, 2, 3}));
  EXPECT_FALSE(NumericVersion({1, 2, 3}) == NumericVersion({1, 2, 4}));
  EXPECT_FALSE(NumericVersion({1, 2, 3}) == NumericVersion({1, 9, 3}));

  EXPECT_FALSE(NumericVersion({1, 2, 3}) == NumericVersion({9, 2, 3}));
}

TEST_F(UtilsTest, NumericVersionOperatorNotEqual) {
  EXPECT_NE(NumericVersion({1, 2, 3}), NumericVersion({1, 2, 4}));
  EXPECT_NE(NumericVersion({1, 2, 3}), NumericVersion({1, 1, 3}));
  EXPECT_NE(NumericVersion({1, 2, 3}), NumericVersion({0, 2, 3}));

  EXPECT_FALSE(NumericVersion{} != NumericVersion{});
}

TEST_F(UtilsTest, NumericVersionOperatorLessThan) {
  EXPECT_LT(NumericVersion({1, 2, 3}), NumericVersion({1, 2, 9}));
  EXPECT_LT(NumericVersion({1, 2, 3}), NumericVersion({1, 9, 3}));
  EXPECT_LT(NumericVersion({1, 2, 3}), NumericVersion({9, 2, 3}));

  EXPECT_FALSE(NumericVersion({1, 2, 3}) < NumericVersion({1, 2, 3}));
  EXPECT_FALSE(NumericVersion({1, 2, 4}) < NumericVersion({1, 2, 3}));
}

TEST_F(UtilsTest, NumericVersionOperatorGreaterThan) {
  EXPECT_GT(NumericVersion({4, 5, 6}), NumericVersion({4, 5, 1}));
  EXPECT_GT(NumericVersion({4, 5, 6}), NumericVersion({4, 1, 6}));
  EXPECT_GT(NumericVersion({4, 5, 6}), NumericVersion({1, 5, 6}));

  EXPECT_FALSE(NumericVersion({9, 9, 9}) > NumericVersion({9, 9, 9}));
  EXPECT_FALSE(NumericVersion({1, 2, 2}) > NumericVersion({1, 2, 4}));
}

TEST_F(UtilsTest, NumericVersionOperatorLessOrEqual) {
  EXPECT_LE(NumericVersion({1, 2, 3}), NumericVersion({1, 2, 3}));
  EXPECT_LE(NumericVersion({1, 2, 3}), NumericVersion({1, 2, 9}));
  EXPECT_LE(NumericVersion({1, 2, 3}), NumericVersion({1, 9, 3}));
  EXPECT_LE(NumericVersion({1, 2, 3}), NumericVersion({9, 2, 3}));

  EXPECT_FALSE(NumericVersion({1, 2, 4}) <= NumericVersion({1, 2, 3}));
}

TEST_F(UtilsTest, NumericVersionOperatorGreaterOrEqual) {
  EXPECT_GE(NumericVersion({4, 5, 6}), NumericVersion({4, 5, 6}));
  EXPECT_GE(NumericVersion({4, 5, 6}), NumericVersion({4, 5, 1}));
  EXPECT_GE(NumericVersion({4, 5, 6}), NumericVersion({4, 1, 6}));
  EXPECT_GE(NumericVersion({4, 5, 6}), NumericVersion({1, 5, 6}));

  EXPECT_FALSE(NumericVersion({1, 2, 2}) >= NumericVersion({1, 2, 4}));
}

TEST_F(UtilsTest, StripAnsiSgrCodes_EmptyString) {
  EXPECT_EQ("", StripAnsiSgrCodes(""));
}

TEST_F(UtilsTest, StripAnsiSgrCodes_SingleAnsiColorCodeAtEnd) {
  std::string s("before\x1b[1;2m");
  EXPECT_EQ("before", StripAnsiSgrCodes(s));
}

TEST_F(UtilsTest, StripAnsiSgrCodes_SingleAnsiColorCodeAtBeginning) {
  std::string s("\x1b[1;2mafter");
  EXPECT_EQ("after", StripAnsiSgrCodes(s));
}

TEST_F(UtilsTest, StripAnsiSgrCodes_SingleAnsiColorCodeInMiddle) {
  std::string s("before\x1b[1;2mafter");
  EXPECT_EQ("beforeafter", StripAnsiSgrCodes(s));
}

TEST_F(UtilsTest, StripAnsiSgrCodes_MultipleAnsiColorCodes) {
  std::string s("\x1b[1mfirst\x1b[1;2msecond\x1b[2m");
  EXPECT_EQ("firstsecond", StripAnsiSgrCodes(s));
}

TEST_F(UtilsTest, StripAnsiSgrCodes_IncompleteAnsiColorCode) {
  EXPECT_EQ("", StripAnsiSgrCodes("\x1b"));
  EXPECT_EQ("", StripAnsiSgrCodes("\x1b["));
  EXPECT_EQ("", StripAnsiSgrCodes("\x1b[1"));
  EXPECT_EQ("", StripAnsiSgrCodes("\x1b[1;"));
  EXPECT_EQ("", StripAnsiSgrCodes("\x1b[1;100"));
}

}  // namespace
