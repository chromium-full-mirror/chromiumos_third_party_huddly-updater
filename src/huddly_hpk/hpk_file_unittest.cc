// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hpk_file.h"

#include <base/files/file_path.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

using ::testing::HasSubstr;

const std::string kHpkTestFilePathString(
    "./src/huddly_hpk/example/example_header.hpk");
const std::string kHpkTestFilePath(kHpkTestFilePathString);
static const std::string kMagicSeparator =
    std::string("\0\n--97da1ea4-803a-4979-8e5d-f2aaa0799f4d--\n", 43);

const std::vector<uint32_t> kExpectedFirmwareVersionNumeric_{1, 2, 18};
const std::string kExpectedFirmwareVersionString_("HuddlyIQ-1.2.18-0");

class HpkFileTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  std::string error_msg;
};

TEST_F(HpkFileTest, CreateHpkWithInvalidPathReturnsNullPtrAndErrorMessage) {
  std::string pathname("Does not exist");
  const auto instance =
      huddly::HpkFile::Create(base::FilePath(pathname), &error_msg);
  EXPECT_EQ(nullptr, instance);

  EXPECT_NE("", error_msg);
  EXPECT_THAT(error_msg, HasSubstr(pathname));
}

TEST_F(HpkFileTest,
       CreateHpkWithNoMagicSeparatorReturnsNullptrAndErrorMessage) {
  std::string file_contents;
  const auto instance = huddly::HpkFile::Create(file_contents, &error_msg);
  EXPECT_EQ(nullptr, instance);

  EXPECT_NE("", error_msg);
  EXPECT_THAT(error_msg, HasSubstr("Magic separator not found"));
}

TEST_F(HpkFileTest, CreateHpkWithInvalidJsonReturnsNullptrAndErrorMessage) {
  std::string file_contents = "{" + kMagicSeparator;
  const auto instance = huddly::HpkFile::Create(file_contents, &error_msg);
  EXPECT_EQ(nullptr, instance);

  EXPECT_NE("", error_msg);
  EXPECT_THAT(error_msg, HasSubstr("Failed to parse manifest"));
}

TEST_F(HpkFileTest, CreateHpkWithValidPathReturnsNonNullPtr) {
  const auto instance =
      huddly::HpkFile::Create(base::FilePath(kHpkTestFilePath), &error_msg);
  EXPECT_NE(nullptr, instance);
  EXPECT_EQ("", error_msg);
}

TEST_F(HpkFileTest, GetManifestData) {
  const auto instance =
      huddly::HpkFile::Create(base::FilePath(kHpkTestFilePath), &error_msg);
  ASSERT_NE(nullptr, instance);
  EXPECT_EQ("", error_msg);

  std::string manifest = instance->GetManifestData();

  EXPECT_LT(0, manifest.size());
  ASSERT_EQ('{', manifest.at(0));
  ASSERT_EQ('}', manifest.at(manifest.size() - 1));
}

TEST_F(HpkFileTest, GetVersionNumericButVersionIsNotInDictionary) {
  std::string file_contents = "{}" + kMagicSeparator;
  const auto instance = huddly::HpkFile::Create(file_contents, &error_msg);
  ASSERT_NE(nullptr, instance);

  EXPECT_EQ("", error_msg);

  std::vector<uint32_t> version;
  EXPECT_FALSE(instance->GetFirmwareVersionNumeric(&version, &error_msg));
  EXPECT_THAT(error_msg, HasSubstr("Failed to get firmware version as list"));
}

TEST_F(HpkFileTest, GetVersionNumericButVersionTooShort) {
  std::string file_contents =
      "{ \"version\": { \"numerical\": [ 1, 2 ] } }" + kMagicSeparator;
  const auto instance = huddly::HpkFile::Create(file_contents, &error_msg);
  ASSERT_NE(nullptr, instance);

  EXPECT_EQ("", error_msg);

  std::vector<uint32_t> version;
  EXPECT_FALSE(instance->GetFirmwareVersionNumeric(&version, &error_msg));
  EXPECT_THAT(error_msg, HasSubstr("Unexpected firmware version length"));
}

TEST_F(HpkFileTest, GetVersionButNumericVersionWrongType) {
  std::string file_contents =
      "{ \"version\": { \"numerical\": [ 1.0, 2.0, 3.0 ] } }" + kMagicSeparator;
  const auto instance = huddly::HpkFile::Create(file_contents, &error_msg);
  ASSERT_NE(nullptr, instance);

  EXPECT_EQ("", error_msg);

  std::vector<uint32_t> version;
  EXPECT_FALSE(instance->GetFirmwareVersionNumeric(&version, &error_msg));
  EXPECT_THAT(error_msg,
              HasSubstr("Failed to get firmware version component as integer"));
}

TEST_F(HpkFileTest, GetVersionNumericFromExampleFile) {
  const auto instance =
      huddly::HpkFile::Create(base::FilePath(kHpkTestFilePath), &error_msg);
  ASSERT_NE(nullptr, instance);
  EXPECT_EQ("", error_msg);

  std::vector<uint32_t> firmware_version;
  EXPECT_TRUE(
      instance->GetFirmwareVersionNumeric(&firmware_version, &error_msg));
  EXPECT_EQ("", error_msg);
  EXPECT_EQ(kExpectedFirmwareVersionNumeric_, firmware_version);
}

TEST_F(HpkFileTest, GetVersionStringFromExampleFile) {
  const auto instance =
      huddly::HpkFile::Create(base::FilePath(kHpkTestFilePath), &error_msg);
  ASSERT_NE(nullptr, instance);
  EXPECT_EQ("", error_msg);

  std::string firmware_version;
  EXPECT_TRUE(
      instance->GetFirmwareVersionString(&firmware_version, &error_msg));
  EXPECT_EQ("", error_msg);
  EXPECT_EQ(kExpectedFirmwareVersionString_, firmware_version);
}

}  // namespace
