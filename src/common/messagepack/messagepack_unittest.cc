// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "messagepack.h"

#include <gtest/gtest.h>
#include <msgpack.hpp>
#include <string>

namespace {

void PackString(msgpack::packer<msgpack::sbuffer>* packer,
                const std::string& string) {
  packer->pack_str(string.size());
  packer->pack_str_body(string.c_str(), string.size());
}

class MessagePackTest : public ::testing::Test {
 protected:
  void PackTestMap() {
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> packer(sbuf);

    test_map_size_ = 5;
    packer.pack_map(test_map_size_);

    PackString(&packer, "bool");
    packer.pack_true();

    PackString(&packer, "uint64");
    packer.pack_uint64(100000);

    PackString(&packer, "int64");
    packer.pack_int64(-100000);

    PackString(&packer, "double");
    packer.pack_double(100.5);

    PackString(&packer, "string");
    PackString(&packer, "str");

    uint8_t* data_begin = reinterpret_cast<uint8_t*>(sbuf.data());
    auto data_end = data_begin + sbuf.size();
    map_packed_ = std::vector<uint8_t>(data_begin, data_end);
  }

  void PackTestInt64() {
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> packer(sbuf);

    packer.pack_int64(kInt64Value_);

    uint8_t* data_begin = reinterpret_cast<uint8_t*>(sbuf.data());
    auto data_end = data_begin + sbuf.size();
    int64_packed_ = std::vector<uint8_t>(data_begin, data_end);
  }

  void PackTestUint64() {
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> packer(sbuf);

    packer.pack_uint64(kUint64Value_);

    uint8_t* data_begin = reinterpret_cast<uint8_t*>(sbuf.data());
    auto data_end = data_begin + sbuf.size();
    uint64_packed_ = std::vector<uint8_t>(data_begin, data_end);
  }

  void PackTestDouble() {
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> packer(sbuf);

    packer.pack_float(kDoubleValue_);

    uint8_t* data_begin = reinterpret_cast<uint8_t*>(sbuf.data());
    auto data_end = data_begin + sbuf.size();
    double_packed_ = std::vector<uint8_t>(data_begin, data_end);
  }

  void PackTestString() {
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> packer(sbuf);

    PackString(&packer, kStringValue_);

    uint8_t* data_begin = reinterpret_cast<uint8_t*>(sbuf.data());
    auto data_end = data_begin + sbuf.size();
    string_packed_ = std::vector<uint8_t>(data_begin, data_end);
  }

  void PackTestArray() {
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> packer(sbuf);

    packer.pack_array(kArrayValues_.size());
    for (const auto& value : kArrayValues_) {
      packer.pack_int(value);
    }

    uint8_t* data_begin = reinterpret_cast<uint8_t*>(sbuf.data());
    auto data_end = data_begin + sbuf.size();
    array_packed_ = std::vector<uint8_t>(data_begin, data_end);
  }

  void PackTestBin(const std::vector<uint8_t> values,
                   std::vector<uint8_t>* packed) {
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> packer(sbuf);

    packer.pack_bin(values.size());
    packer.pack_bin_body(reinterpret_cast<const char*>(values.data()),
                         values.size());

    uint8_t* data_begin = reinterpret_cast<uint8_t*>(sbuf.data());
    auto data_end = data_begin + sbuf.size();
    *packed = std::vector<uint8_t>(data_begin, data_end);
  }

  void SetUp() override {
    PackTestMap();
    PackTestInt64();
    PackTestUint64();
    PackTestDouble();
    PackTestString();
    PackTestArray();
    PackTestBin(kBinValues_, &bin_packed_);
    PackTestBin({}, &bin_packed_empty_);
  }

  std::vector<uint8_t> map_packed_;
  int test_map_size_;
  std::vector<uint8_t> uint64_packed_;
  const uint64_t kUint64Value_ = 1234567890123;
  std::vector<uint8_t> int64_packed_;
  const int64_t kInt64Value_ = -9876543210987;
  std::vector<uint8_t> double_packed_;
  const double kDoubleValue_ = 1.23455e-2;

  const std::vector<uint8_t> nil_packed_ = {0xc0};
  const std::vector<uint8_t> false_packed_ = {0xc2};
  const std::vector<uint8_t> true_packed_ = {0xc3};
  const std::vector<uint8_t> unsigned_fixint_packed_ = {0x05};
  const int kUnsignedFixintValue_ = 5;
  std::vector<uint8_t> string_packed_;
  const std::string kStringValue_{"test"};
  const std::vector<int64_t> kArrayValues_{1, 100, -100};
  std::vector<uint8_t> array_packed_;
  const std::vector<uint8_t> kBinValues_{42, 128, 81, 255, 0};
  std::vector<uint8_t> bin_packed_;
  std::vector<uint8_t> bin_packed_empty_;
};  // namespace

TEST_F(MessagePackTest, CreateUnpackerEmptyPackedReturnsNullptr) {
  const auto unpacker =
      huddly::messagepack::Unpacker::Create(std::vector<uint8_t>());
  EXPECT_EQ(nullptr, unpacker);
}

TEST_F(MessagePackTest, CreateUnpackerInvalidDataReturnsNullptr) {
  // 0xa3 is string of length 3, but only one byte of string data provided.
  const std::vector<uint8_t> invalid = {0xa3, 0x01};
  const auto unpacker = huddly::messagepack::Unpacker::Create(invalid);
  ASSERT_EQ(nullptr, unpacker);
}

TEST_F(MessagePackTest, CreateUnpackerTooMuchPackedDataIsIgnored) {
  // 0xa1 is string of length 1, but 2 bytes follow.
  const std::vector<uint8_t> too_long = {0xa1, 0x01, 0x02};
  const auto unpacker = huddly::messagepack::Unpacker::Create(too_long);
  EXPECT_NE(nullptr, unpacker);
}  // namespace

TEST_F(MessagePackTest, CreateUnpackerWithValidData) {
  const auto unpacker = huddly::messagepack::Unpacker::Create(map_packed_);
  EXPECT_NE(nullptr, unpacker);
}

TEST_F(MessagePackTest, UnpackerGetObject) {
  const auto unpacker = huddly::messagepack::Unpacker::Create(map_packed_);
  ASSERT_NE(nullptr, unpacker);

  const auto object = unpacker->GetRootObject();
  EXPECT_TRUE(object.Is<huddly::messagepack::Map>());
}

TEST_F(MessagePackTest, ObjectDefaultConstructed) {
  huddly::messagepack::Object object;
  EXPECT_TRUE(object.IsNil());
}

TEST_F(MessagePackTest, ObjectNil) {
  const auto unpacker = huddly::messagepack::Unpacker::Create(nil_packed_);
  ASSERT_NE(nullptr, unpacker);

  const auto object = unpacker->GetRootObject();
  EXPECT_TRUE(object.IsNil());
  EXPECT_FALSE(object.Is<bool>());
  EXPECT_FALSE(object.Is<double>());
  EXPECT_FALSE(object.Is<int64_t>());
  EXPECT_FALSE(object.Is<std::string>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Map>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Array>());

  std::string to_string_value;
  EXPECT_TRUE(object.GetAs<std::string>(&to_string_value));
  EXPECT_EQ("nil", to_string_value);
}

TEST_F(MessagePackTest, ObjectBoolFalse) {
  const auto unpacker = huddly::messagepack::Unpacker::Create(false_packed_);
  ASSERT_NE(nullptr, unpacker);

  const auto object = unpacker->GetRootObject();
  EXPECT_FALSE(object.IsNil());
  EXPECT_TRUE(object.Is<bool>());
  EXPECT_FALSE(object.Is<double>());
  EXPECT_FALSE(object.Is<int64_t>());
  EXPECT_FALSE(object.Is<std::string>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Map>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Array>());

  bool value = true;
  EXPECT_TRUE(object.Get<bool>(&value));
  EXPECT_FALSE(value);

  std::string string_value;
  EXPECT_FALSE(object.Get<std::string>(&string_value));

  std::string to_string_value;
  EXPECT_TRUE(object.GetAs<std::string>(&to_string_value));
  EXPECT_EQ("false", to_string_value);
}

TEST_F(MessagePackTest, ObjectBoolTrue) {
  const auto unpacker = huddly::messagepack::Unpacker::Create(true_packed_);
  ASSERT_NE(nullptr, unpacker);

  const auto object = unpacker->GetRootObject();

  // Only do value test here, since type is the same as in ObjectBoolFalse.
  bool value = false;
  EXPECT_TRUE(object.Get<bool>(&value));
  EXPECT_TRUE(value);

  std::string to_string_value;
  EXPECT_TRUE(object.GetAs<std::string>(&to_string_value));
  EXPECT_EQ("true", to_string_value);
}

TEST_F(MessagePackTest, ObjectSignedInt) {
  const auto unpacker = huddly::messagepack::Unpacker::Create(int64_packed_);

  const auto object = unpacker->GetRootObject();
  EXPECT_FALSE(object.IsNil());
  EXPECT_FALSE(object.Is<bool>());
  EXPECT_FALSE(object.Is<double>());
  EXPECT_TRUE(object.Is<int64_t>());
  EXPECT_FALSE(object.Is<uint64_t>());
  EXPECT_FALSE(object.Is<std::string>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Map>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Array>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Bin>());

  int64_t value;
  EXPECT_TRUE(object.Get<int64_t>(&value));
  EXPECT_EQ(kInt64Value_, value);
  EXPECT_TRUE(object.GetAs<int64_t>(&value));
  EXPECT_EQ(kInt64Value_, value);

  double double_value;
  EXPECT_TRUE(object.GetAs<double>(&double_value));
  EXPECT_DOUBLE_EQ(kInt64Value_, double_value);

  std::string to_string_value;
  EXPECT_TRUE(object.GetAs<std::string>(&to_string_value));
  EXPECT_EQ(std::to_string(kInt64Value_), to_string_value);
}

TEST_F(MessagePackTest, ObjectUnsignedInt) {
  const auto unpacker = huddly::messagepack::Unpacker::Create(uint64_packed_);
  ASSERT_NE(nullptr, unpacker);

  const auto object = unpacker->GetRootObject();
  EXPECT_FALSE(object.IsNil());
  EXPECT_FALSE(object.Is<bool>());
  EXPECT_FALSE(object.Is<double>());
  EXPECT_FALSE(object.Is<int64_t>());
  EXPECT_TRUE(object.Is<uint64_t>());
  EXPECT_FALSE(object.Is<std::string>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Map>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Array>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Bin>());

  uint64_t value;
  EXPECT_TRUE(object.Get<uint64_t>(&value));
  EXPECT_EQ(kUint64Value_, value);
  EXPECT_TRUE(object.GetAs<uint64_t>(&value));
  EXPECT_EQ(kUint64Value_, value);

  double double_value;
  EXPECT_TRUE(object.GetAs<double>(&double_value));
  EXPECT_DOUBLE_EQ(kUint64Value_, double_value);
}

TEST_F(MessagePackTest, ObjectUnsignedFixedInt) {
  const auto unpacker =
      huddly::messagepack::Unpacker::Create(unsigned_fixint_packed_);
  ASSERT_NE(nullptr, unpacker);

  const auto object = unpacker->GetRootObject();
  EXPECT_FALSE(object.IsNil());
  EXPECT_FALSE(object.Is<bool>());
  EXPECT_FALSE(object.Is<double>());
  EXPECT_FALSE(object.Is<int64_t>());
  EXPECT_TRUE(object.Is<uint64_t>());
  EXPECT_FALSE(object.Is<std::string>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Map>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Array>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Bin>());

  uint64_t value;
  EXPECT_TRUE(object.Get<uint64_t>(&value));
  EXPECT_EQ(kUnsignedFixintValue_, value);
  EXPECT_TRUE(object.GetAs<uint64_t>(&value));
  EXPECT_EQ(kUnsignedFixintValue_, value);

  double double_value;
  EXPECT_TRUE(object.GetAs<double>(&double_value));
  EXPECT_DOUBLE_EQ(kUnsignedFixintValue_, double_value);
}

TEST_F(MessagePackTest, ObjectUnsignedDouble) {
  const auto unpacker = huddly::messagepack::Unpacker::Create(double_packed_);
  ASSERT_NE(nullptr, unpacker);

  const auto object = unpacker->GetRootObject();
  EXPECT_FALSE(object.IsNil());
  EXPECT_FALSE(object.Is<bool>());
  EXPECT_TRUE(object.Is<double>());
  EXPECT_FALSE(object.Is<int64_t>());
  EXPECT_FALSE(object.Is<uint64_t>());
  EXPECT_FALSE(object.Is<std::string>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Map>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Array>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Bin>());

  double value = 0.0;
  EXPECT_TRUE(object.Get<double>(&value));
  EXPECT_NEAR(kDoubleValue_, value, 1e-4);
  EXPECT_TRUE(object.GetAs<double>(&value));
  EXPECT_NEAR(kDoubleValue_, value, 1e-4);

  int64_t int64_value;
  EXPECT_FALSE(object.GetAs<int64_t>(&int64_value));

  std::string to_string_value;
  EXPECT_TRUE(object.GetAs<std::string>(&to_string_value));
  EXPECT_EQ(std::to_string(kDoubleValue_), to_string_value);
}

TEST_F(MessagePackTest, ObjectString) {
  const auto unpacker = huddly::messagepack::Unpacker::Create(string_packed_);
  ASSERT_NE(nullptr, unpacker);

  const auto object = unpacker->GetRootObject();
  EXPECT_FALSE(object.IsNil());
  EXPECT_FALSE(object.Is<bool>());
  EXPECT_FALSE(object.Is<double>());
  EXPECT_FALSE(object.Is<int64_t>());
  EXPECT_TRUE(object.Is<std::string>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Map>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Array>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Bin>());

  std::string value;
  EXPECT_TRUE(object.Get<std::string>(&value));
  EXPECT_EQ(kStringValue_, value);

  EXPECT_TRUE(object.GetAs<std::string>(&value));
  EXPECT_EQ(kStringValue_, value);
}

TEST_F(MessagePackTest, MapDefaultConstructed) {
  huddly::messagepack::Map map;

  // Make sure default constructed map is safe to access
  EXPECT_EQ(0, map.Size());
  EXPECT_FALSE(map.GetValueObject("test", nullptr));
  EXPECT_FALSE(map.GetValueAs<int64_t>("asdf", nullptr));
}

TEST_F(MessagePackTest, Map) {
  const auto unpacker = huddly::messagepack::Unpacker::Create(map_packed_);
  ASSERT_NE(nullptr, unpacker);

  const auto root_object = unpacker->GetRootObject();
  EXPECT_FALSE(root_object.IsNil());
  EXPECT_FALSE(root_object.Is<bool>());
  EXPECT_FALSE(root_object.Is<double>());
  EXPECT_FALSE(root_object.Is<int64_t>());
  EXPECT_FALSE(root_object.Is<std::string>());
  EXPECT_TRUE(root_object.Is<huddly::messagepack::Map>());
  EXPECT_FALSE(root_object.Is<huddly::messagepack::Array>());
  EXPECT_FALSE(root_object.Is<huddly::messagepack::Bin>());

  huddly::messagepack::Map map;

  EXPECT_TRUE(root_object.Get<huddly::messagepack::Map>(&map));
  EXPECT_EQ(test_map_size_, map.Size());

  huddly::messagepack::Object object;
  EXPECT_FALSE(map.GetValueObject("DoesNotExist", &object));

  std::string string_value;
  EXPECT_TRUE(map.GetValueObject("string", &object));
  EXPECT_TRUE(object.Is<std::string>());
  EXPECT_TRUE(object.Get<std::string>(&string_value));

  EXPECT_TRUE(map.GetValue("string", &string_value));
  EXPECT_EQ("str", string_value);

  double double_value;
  EXPECT_TRUE(map.GetValue<double>("double", &double_value));
  EXPECT_EQ(100.5, double_value);
  EXPECT_TRUE(map.GetValueAs<double>("double", &double_value));

  int64_t int64_value;
  // No conversion from double to int64_t. Expect false.
  EXPECT_FALSE(map.GetValueAs<int64_t>("double", &int64_value));
  // TODO(torleiv): Remove unused entries in the test map.
}

TEST_F(MessagePackTest, ArrayDefaultConstructed) {
  huddly::messagepack::Array array;

  // Make sure it is safe to access default constructed array.
  EXPECT_EQ(0, array.Size());
  std::vector<huddly::messagepack::Object> objects;
  EXPECT_TRUE(array.GetValueObjects(&objects));
  EXPECT_EQ(0, objects.size());
  std::vector<double> doubles;
  EXPECT_TRUE(array.GetValuesAs<double>(&doubles));
  EXPECT_EQ(0, objects.size());
}

TEST_F(MessagePackTest, Array) {
  const auto unpacker = huddly::messagepack::Unpacker::Create(array_packed_);
  ASSERT_NE(nullptr, unpacker);

  const auto object = unpacker->GetRootObject();
  EXPECT_FALSE(object.IsNil());
  EXPECT_FALSE(object.Is<bool>());
  EXPECT_FALSE(object.Is<double>());
  EXPECT_FALSE(object.Is<int64_t>());
  EXPECT_FALSE(object.Is<std::string>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Map>());
  EXPECT_TRUE(object.Is<huddly::messagepack::Array>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Bin>());

  huddly::messagepack::Array array;

  EXPECT_TRUE(object.Get<huddly::messagepack::Array>(&array));
  EXPECT_EQ(kArrayValues_.size(), array.Size());

  EXPECT_TRUE(object.GetAs<huddly::messagepack::Array>(&array));
  EXPECT_EQ(kArrayValues_.size(), array.Size());

  std::vector<huddly::messagepack::Object> object_array;
  EXPECT_TRUE(array.GetValueObjects(&object_array));
  EXPECT_TRUE(object_array[0].Is<uint64_t>());

  std::vector<int64_t> int_array;
  // Values in array are not homogeneous, so this is expected to return false.
  EXPECT_FALSE(array.GetValues<int64_t>(&int_array));

  EXPECT_TRUE(array.GetValuesAs<int64_t>(&int_array));
  EXPECT_EQ(kArrayValues_, int_array);

  std::vector<double> double_array;
  EXPECT_TRUE(array.GetValuesAs<double>(&double_array));
}

TEST_F(MessagePackTest, BinDefaultConstructed) {
  huddly::messagepack::Bin bin;

  // Make sure it is safe to access default constructed array.
  EXPECT_EQ(0, bin.Size());
  std::vector<huddly::messagepack::Object> objects;
  std::vector<uint8_t> bytes;
  EXPECT_TRUE(bin.GetValues(&bytes));
  EXPECT_EQ(0, objects.size());
}

TEST_F(MessagePackTest, Bin) {
  const auto unpacker = huddly::messagepack::Unpacker::Create(bin_packed_);
  ASSERT_NE(nullptr, unpacker);

  const auto object = unpacker->GetRootObject();
  EXPECT_FALSE(object.IsNil());
  EXPECT_FALSE(object.Is<bool>());
  EXPECT_FALSE(object.Is<double>());
  EXPECT_FALSE(object.Is<int64_t>());
  EXPECT_FALSE(object.Is<std::string>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Map>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Array>());
  EXPECT_TRUE(object.Is<huddly::messagepack::Bin>());

  huddly::messagepack::Bin bin;

  EXPECT_TRUE(object.Get<huddly::messagepack::Bin>(&bin));
  EXPECT_EQ(kBinValues_.size(), bin.Size());

  EXPECT_TRUE(object.GetAs<huddly::messagepack::Bin>(&bin));
  EXPECT_EQ(kBinValues_.size(), bin.Size());

  std::vector<uint8_t> values;

  EXPECT_TRUE(bin.GetValues(&values));
  EXPECT_EQ(kBinValues_, values);
}

TEST_F(MessagePackTest, BinEmpty) {
  const auto unpacker =
      huddly::messagepack::Unpacker::Create(bin_packed_empty_);
  ASSERT_NE(nullptr, unpacker);

  const auto object = unpacker->GetRootObject();
  EXPECT_FALSE(object.IsNil());
  EXPECT_FALSE(object.Is<bool>());
  EXPECT_FALSE(object.Is<double>());
  EXPECT_FALSE(object.Is<int64_t>());
  EXPECT_FALSE(object.Is<std::string>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Map>());
  EXPECT_FALSE(object.Is<huddly::messagepack::Array>());
  EXPECT_TRUE(object.Is<huddly::messagepack::Bin>());

  huddly::messagepack::Bin bin;

  EXPECT_TRUE(object.Get<huddly::messagepack::Bin>(&bin));
  EXPECT_EQ(0, bin.Size());

  std::vector<uint8_t> values;

  EXPECT_TRUE(bin.GetValues(&values));
  EXPECT_EQ(std::vector<uint8_t>{}, values);
}
}  // namespace
