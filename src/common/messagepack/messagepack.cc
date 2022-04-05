// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "messagepack.h"

#include <base/logging.h>

// Macro for telling -Wimplicit-fallthrough that a fallthrough is intentional.
// This is needed for android nyc builds
#if __ANDROID__
#if !defined(FALLTHROUGH) && defined(__clang__)
#define FALLTHROUGH [[clang::fallthrough]]
#endif
#endif

namespace huddly {
namespace messagepack {

constexpr int kObjectPrintBufferSize = 100 * 1024;

Object::Object() {
  object_.type = MSGPACK_OBJECT_NIL;
}

bool Object::IsNil() const {
  return object_.type == MSGPACK_OBJECT_NIL;
}

template <>
bool Object::Is<bool>() const {
  return object_.type == MSGPACK_OBJECT_BOOLEAN;
}

template <>
bool Object::Is<double>() const {
  return (object_.type == MSGPACK_OBJECT_FLOAT ||
          object_.type == MSGPACK_OBJECT_FLOAT32);
}

template <>
bool Object::Is<int64_t>() const {
  // The term NEGATIVE_INTEGER used in msgpack-c is somewhat of a misnomer.
  // Everywhere in the source code, it refers to the value accessed via the i64
  // member, so it would probably better be called SIGNED_INTEGER.
  return (object_.type == MSGPACK_OBJECT_NEGATIVE_INTEGER);
}

template <>
bool Object::Is<uint64_t>() const {
  // The term POSITIVE_INTEGER used in msgpack-c is somewhat of a misnomer.
  // Everywhere in the source code, it refers to the value accessed via the u64
  // member, so it would probably better be called UNSIGNED_INTEGER.
  return (object_.type == MSGPACK_OBJECT_POSITIVE_INTEGER);
}

template <>
bool Object::Is<std::string>() const {
  return object_.type == MSGPACK_OBJECT_STR;
}

template <>
bool Object::Is<Map>() const {
  return object_.type == MSGPACK_OBJECT_MAP;
}

template <>
bool Object::Is<Array>() const {
  return object_.type == MSGPACK_OBJECT_ARRAY;
}

template <>
bool Object::Is<Bin>() const {
  return object_.type == MSGPACK_OBJECT_BIN;
}

template <>
bool Object::Get<bool>(bool* out) const {
  if (!Is<bool>())
    return false;
  *out = object_.via.boolean;
  return true;
}

template <>
bool Object::Get<double>(double* out) const {
  if (!Is<double>())
    return false;
  *out = object_.via.f64;
  return true;
}

template <>
bool Object::Get<int64_t>(int64_t* out) const {
  if (!Is<int64_t>())
    return false;

  *out = object_.via.i64;
  return true;
}

template <>
bool Object::Get<uint64_t>(uint64_t* out) const {
  if (!Is<uint64_t>())
    return false;
  *out = object_.via.u64;
  return true;
}

template <>
bool Object::Get<std::string>(std::string* out) const {
  if (!Is<std::string>())
    return false;
  *out = std::string(object_.via.str.ptr, object_.via.str.size);
  return true;
}

template <>
bool Object::Get<Map>(Map* out) const {
  if (!Is<Map>())
    return false;
  *out = Map(object_.via.map);
  return true;
}

template <>
bool Object::Get<Array>(Array* out) const {
  if (!Is<Array>())
    return false;
  *out = Array(object_.via.array);
  return true;
}

template <>
bool Object::Get<Bin>(Bin* out) const {
  if (!Is<Bin>())
    return false;
  *out = Bin(object_.via.bin);
  return true;
}

template <>
bool Object::GetAs<int64_t>(int64_t* out) const {
  switch (object_.type) {
    case MSGPACK_OBJECT_POSITIVE_INTEGER:
      *out = static_cast<int64_t>(object_.via.u64);
      return true;
    case MSGPACK_OBJECT_NEGATIVE_INTEGER:
      *out = object_.via.i64;
      return true;
    case MSGPACK_OBJECT_FLOAT32:
      [[fallthrough]];
    case MSGPACK_OBJECT_FLOAT:
      [[fallthrough]];
    case MSGPACK_OBJECT_BOOLEAN:
      [[fallthrough]];
    case MSGPACK_OBJECT_NIL:
      [[fallthrough]];
    case MSGPACK_OBJECT_STR:
      [[fallthrough]];
    case MSGPACK_OBJECT_ARRAY:
      [[fallthrough]];
    case MSGPACK_OBJECT_MAP:
      [[fallthrough]];
    case MSGPACK_OBJECT_BIN:
      [[fallthrough]];
    case MSGPACK_OBJECT_EXT:
      return false;
  }
}

template <>
bool Object::GetAs<uint64_t>(uint64_t* out) const {
  switch (object_.type) {
    case MSGPACK_OBJECT_POSITIVE_INTEGER:
      *out = object_.via.u64;
      return true;
    case MSGPACK_OBJECT_NEGATIVE_INTEGER:
      *out = static_cast<uint64_t>(object_.via.i64);
      return true;
    case MSGPACK_OBJECT_FLOAT32:
      [[fallthrough]];
    case MSGPACK_OBJECT_FLOAT:
      [[fallthrough]];
    case MSGPACK_OBJECT_BOOLEAN:
      [[fallthrough]];
    case MSGPACK_OBJECT_NIL:
      [[fallthrough]];
    case MSGPACK_OBJECT_STR:
      [[fallthrough]];
    case MSGPACK_OBJECT_ARRAY:
      [[fallthrough]];
    case MSGPACK_OBJECT_MAP:
      [[fallthrough]];
    case MSGPACK_OBJECT_BIN:
      [[fallthrough]];
    case MSGPACK_OBJECT_EXT:
      return false;
  }
}

template <>
bool Object::GetAs<double>(double* out) const {
  switch (object_.type) {
    case MSGPACK_OBJECT_POSITIVE_INTEGER:
      *out = static_cast<double>(object_.via.u64);
      return true;
    case MSGPACK_OBJECT_NEGATIVE_INTEGER:
      *out = static_cast<double>(object_.via.i64);
      return true;
    case MSGPACK_OBJECT_FLOAT32:
      [[fallthrough]];
    case MSGPACK_OBJECT_FLOAT:
      *out = object_.via.f64;
      return true;
    case MSGPACK_OBJECT_BOOLEAN:
      [[fallthrough]];
    case MSGPACK_OBJECT_NIL:
      [[fallthrough]];
    case MSGPACK_OBJECT_STR:
      [[fallthrough]];
    case MSGPACK_OBJECT_ARRAY:
      [[fallthrough]];
    case MSGPACK_OBJECT_MAP:
      [[fallthrough]];
    case MSGPACK_OBJECT_BIN:
      [[fallthrough]];
    case MSGPACK_OBJECT_EXT:
      return false;
  }
}

template <>
bool Object::GetAs<std::string>(std::string* out) const {
  if (Is<std::string>()) {
    return Get<std::string>(out);
  }

  char buffer[kObjectPrintBufferSize];
  int string_length =
      msgpack_object_print_buffer(buffer, sizeof(buffer), object_);
  if (string_length <= 0 || string_length >= sizeof(buffer)) {
    LOG(ERROR) << "Failed to convert object to string";
    return false;
  }
  if (string_length < 0) {
    LOG(ERROR) << "Unexpected return value: " << string_length;
    return false;
  }
  *out = std::string(buffer, string_length);
  return true;
}

std::string Object::ToString() const {
  std::string out;
  if (!GetAs<std::string>(&out)) {
    out = "";
  }
  return out;
}

std::string Object::TypeName() const {
  switch (object_.type) {
    case MSGPACK_OBJECT_NIL:
      return "Nil";
    case MSGPACK_OBJECT_BOOLEAN:
      return "Boolean";
    case MSGPACK_OBJECT_POSITIVE_INTEGER:
      return "Int";
    case MSGPACK_OBJECT_NEGATIVE_INTEGER:
      return "Uint";
    case MSGPACK_OBJECT_FLOAT32:
      return "Float32";
    case MSGPACK_OBJECT_FLOAT64:
      return "Float64";
    case MSGPACK_OBJECT_STR:
      return "String";
    case MSGPACK_OBJECT_ARRAY:
      return "Array";
    case MSGPACK_OBJECT_MAP:
      return "Map";
    case MSGPACK_OBJECT_BIN:
      return "Bin";
    case MSGPACK_OBJECT_EXT:
      return "Object ext";
    default:
      return "Unknown type";
  }
}

Map::Map() {
  map_.size = 0;
  map_.ptr = nullptr;
}

int Map::Size() const {
  return map_.size;
}

bool Map::GetValueObject(const std::string& key, Object* out) const {
  msgpack_object_kv key_value;
  if (!FindKeyValue(key, &key_value)) {
    return false;
  }
  *out = Object(key_value.val);
  return true;
}

bool Map::FindKeyValue(const std::string& key,
                       msgpack_object_kv* key_value) const {
  const auto map_begin = map_.ptr;
  const auto map_end = map_begin + map_.size;
  auto find_it = std::find_if(
      map_begin, map_end, [&key](const msgpack_object_kv& key_value) {
        Object candidate_key_object(key_value.key);
        std::string candidate_key;
        if (!candidate_key_object.Get<std::string>(&candidate_key))
          return false;
        return key == candidate_key;
      });
  if (find_it == map_end)
    return false;

  *key_value = *find_it;

  return true;
}

std::string Map::ToString() const {
  msgpack_object obj;
  obj.type = MSGPACK_OBJECT_MAP;
  obj.via.map = map_;
  return Object(obj).ToString();
}

Array::Array() {
  array_.size = 0;
  array_.ptr = nullptr;
}

int Array::Size() const {
  return array_.size;
}

bool Array::GetValueObjects(std::vector<Object>* out) const {
  for (auto i = 0; i < Size(); i++) {
    out->push_back(Object(array_.ptr[i]));
  }
  return true;
}

std::string Array::ToString() const {
  msgpack_object obj;
  obj.type = MSGPACK_OBJECT_ARRAY;
  obj.via.array = array_;
  return Object(obj).ToString();
}

Bin::Bin() {
  bin_.size = 0;
  bin_.ptr = nullptr;
}

int Bin::Size() const {
  return bin_.size;
}

std::string Bin::ToString() const {
  msgpack_object obj;
  obj.type = MSGPACK_OBJECT_BIN;
  obj.via.bin = bin_;
  return Object(obj).ToString();
}

Unpacker::Unpacker(const std::vector<uint8_t>& packed) : packed_(packed) {
  msgpack_unpacked_init(&unpacked_);
}

std::unique_ptr<Unpacker> Unpacker::Create(const std::vector<uint8_t>& packed) {
  auto unpacker = std::unique_ptr<Unpacker>(new Unpacker(packed));

  if (packed.empty()) {
    return nullptr;
  }

  size_t offset = 0;
  auto status = msgpack_unpack_next(
      &unpacker->unpacked_,
      reinterpret_cast<const char*>(unpacker->packed_.data()), packed.size(),
      &offset);
  if (status != MSGPACK_UNPACK_SUCCESS) {
    return nullptr;
  }

  return unpacker;
}

Unpacker::~Unpacker() {
  msgpack_unpacked_destroy(&unpacked_);
}

Object Unpacker::GetRootObject() const {
  return Object(unpacked_.data);
}

}  // namespace messagepack
}  // namespace huddly
