// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "manifest.h"

#include <stdlib.h>

#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/values.h>
#include <gtest/gtest.h>

namespace {

const char kManifestJsonFile[] = "./src/huddly_go/example/manifest.json";

class ManifestTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const base::FilePath path(FILE_PATH_LITERAL(kManifestJsonFile));
    manifest.set_path(path);
  }

  huddly::Manifest manifest;
};

TEST_F(ManifestTest, FileExists) {
  EXPECT_TRUE(base::PathExists(manifest.path()));
}

TEST_F(ManifestTest, ReadJSONDictionary) {
  // See also: chromium/src/base/+/master/json/json_reader_unittest.cc

  std::string content;
  ASSERT_TRUE(ReadFileToString(manifest.path(), &content));

  auto root = base::JSONReader::ReadAndReturnValueWithError(
      content, base::JSON_PARSE_RFC);
  ASSERT_TRUE(root.has_value()) << root.error().message;
  EXPECT_TRUE(root->is_dict());
}

TEST_F(ManifestTest, TestParseFile) {
  EXPECT_TRUE(manifest.ParseFile());

  // manifest get methods return empty strings when corresponding fields
  // do not exist.
  EXPECT_EQ(manifest.path().value(), FILE_PATH_LITERAL(kManifestJsonFile));
  EXPECT_EQ(manifest.app_ver(), "0.5.1");
  EXPECT_EQ(manifest.boot_ver(), "0.2.1");
  EXPECT_EQ(manifest.hw_rev(), "6");
}

}  // namespace
