// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SRC_COMMON_MESSAGEPACK_MESSAGEPACK_H_
#define SRC_COMMON_MESSAGEPACK_MESSAGEPACK_H_

#include <msgpack.h>

#include <memory>
#include <string>
#include <vector>

#include <msgpack.h>

namespace huddly {
namespace messagepack {

class Map;
class Array;
class Bin;

class Object {
 public:
  Object();
  explicit Object(const msgpack_object& object) : object_(object) {}

  bool IsNil() const;

  // Is<T> returns true if the underlying type is exactly the same as T
  template <typename T>
  bool Is() const {
    return false;
  }

  // Get<T> returns the object value as T, if the underlying type is T.
  template <typename T>
  bool Get(T* out) const {
    // If no fully specialized template function is defined, the function
    // returns false.
    return false;
  }

  // GetAs<T> attempts to return the object value converted to T. Reasonable
  // numeric casts are performed automatically. Conversions to string is
  // supported, but not from string to any other types. Boolean values are never
  // converted.
  template <typename T>
  bool GetAs(T* out) const {
    // If no fully specialized template is defined, us the Get<T> function.
    return Get<T>(out);
  }

  // Convenience function
  std::string ToString() const;
  std::string TypeName() const;

 private:
  msgpack_object object_;
};

template <>
bool Object::Is<bool>() const;
template <>
bool Object::Is<double>() const;
template <>
bool Object::Is<int64_t>() const;
template <>
bool Object::Is<uint64_t>() const;
template <>
bool Object::Is<std::string>() const;
template <>
bool Object::Is<Map>() const;
template <>
bool Object::Is<Array>() const;
template <>
bool Object::Is<Bin>() const;

template <>
bool Object::Get<bool>(bool* out) const;
template <>
bool Object::Get<double>(double* out) const;
template <>
bool Object::Get<int64_t>(int64_t* out) const;
template <>
bool Object::Get<uint64_t>(uint64_t* out) const;
template <>
bool Object::Get<std::string>(std::string* out) const;
template <>
bool Object::Get<Map>(Map* out) const;
template <>
bool Object::Get<Array>(Array* out) const;
template <>
bool Object::Get<Bin>(Bin* out) const;

template <>
bool Object::GetAs<int64_t>(int64_t* out) const;
template <>
bool Object::GetAs<uint64_t>(uint64_t* out) const;
template <>
bool Object::GetAs<double>(double* out) const;
template <>
bool Object::GetAs<std::string>(std::string* out) const;

class Map {
 public:
  Map();
  explicit Map(const msgpack_object_map& map) : map_(map) {}
  int Size() const;
  bool GetValueObject(const std::string& key, Object* out) const;

  template <typename T>
  bool GetValue(const std::string& key, T* out) const {
    Object value_object;
    if (!GetValueObject(key, &value_object))
      return false;
    if (!value_object.Get<T>(out))
      return false;
    return true;
  }

  template <typename T>
  bool GetValueAs(const std::string& key, T* out) const {
    Object value_object;
    if (!GetValueObject(key, &value_object))
      return false;
    if (!value_object.GetAs<T>(out))
      return false;
    return true;
  }

  std::string ToString() const;

 private:
  bool FindKeyValue(const std::string& key, msgpack_object_kv* key_value) const;
  msgpack_object_map map_;
};

class Array {
 public:
  Array();
  explicit Array(const msgpack_object_array& array) : array_(array) {}
  int Size() const;
  bool GetValueObjects(std::vector<Object>* out) const;

  template <typename T>
  bool GetValues(std::vector<T>* out) const {
    for (auto i = 0u; i < Size(); i++) {
      T value;
      Object object(array_.ptr[i]);
      if (!object.Get<T>(&value))
        return false;
      out->push_back(value);
    }
    return true;
  }

  template <typename T>
  bool GetValuesAs(std::vector<T>* out) const {
    for (auto i = 0u; i < Size(); i++) {
      T value;
      Object object(array_.ptr[i]);
      if (!object.GetAs<T>(&value))
        return false;
      out->push_back(value);
    }
    return true;
  }

  std::string ToString() const;

 private:
  msgpack_object_array array_;
};

class Bin {
 public:
  Bin();
  explicit Bin(const msgpack_object_bin& bin) : bin_(bin) {}
  int Size() const;

  bool GetValues(std::vector<uint8_t>* out) const {
    for (auto i = 0u; i < Size(); i++) {
      out->push_back(reinterpret_cast<const uint8_t*>(bin_.ptr)[i]);
    }
    return true;
  }

  std::string ToString() const;

 private:
  msgpack_object_bin bin_;
};

// The unpacker will keep a copy of the packed messagepack data. However,
// any non-value unpacked data (Object, Map, Array) may point to data in the
// buffer kept in unpacker. The client must keep the Unpacker object alive
// throughout the lifetime of all non-value variables that originates from the
// Unpacker.
class Unpacker {
 public:
  static std::unique_ptr<Unpacker> Create(const std::vector<uint8_t>& packed);
  Object GetRootObject() const;
  template <typename T>
  bool GetRoot(T* out) const {
    return GetRootObject().Get<T>(out);
  }

  Unpacker(const Unpacker&) = delete;
  Unpacker& operator=(const Unpacker&) = delete;

  ~Unpacker();

 private:
  explicit Unpacker(const std::vector<uint8_t>& packed);

  msgpack_unpacked unpacked_;
  std::vector<uint8_t> packed_;
};

}  // namespace messagepack
}  // namespace huddly

#endif  // SRC_COMMON_MESSAGEPACK_MESSAGEPACK_H_
