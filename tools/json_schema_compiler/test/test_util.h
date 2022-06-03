// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_JSON_SCHEMA_COMPILER_TEST_TEST_UTIL_H_
#define TOOLS_JSON_SCHEMA_COMPILER_TEST_TEST_UTIL_H_

#include <memory>

#include "base/strings/string_piece.h"
#include "base/values.h"

namespace json_schema_compiler {
namespace test_util {

base::Value ReadJson(const base::StringPiece& json);

template <typename T>
std::vector<T> Vector(const T& a) {
  std::vector<T> arr;
  arr.push_back(a);
  return arr;
}
template <typename T>
std::vector<T> Vector(const T& a, const T& b) {
  std::vector<T> arr = Vector(a);
  arr.push_back(b);
  return arr;
}
template <typename T>
std::vector<T> Vector(const T& a, const T& b, const T& c) {
  std::vector<T> arr = Vector(a, b);
  arr.push_back(c);
  return arr;
}

std::unique_ptr<base::ListValue> List(std::unique_ptr<base::Value> a);
std::unique_ptr<base::ListValue> List(std::unique_ptr<base::Value> a,
                                      std::unique_ptr<base::Value> b);
std::unique_ptr<base::ListValue> List(std::unique_ptr<base::Value> a,
                                      std::unique_ptr<base::Value> b,
                                      std::unique_ptr<base::Value> c);

std::unique_ptr<base::DictionaryValue> Dictionary(
    const std::string& ak,
    std::unique_ptr<base::Value> av);
std::unique_ptr<base::DictionaryValue> Dictionary(
    const std::string& ak,
    std::unique_ptr<base::Value> av,
    const std::string& bk,
    std::unique_ptr<base::Value> bv);
std::unique_ptr<base::DictionaryValue> Dictionary(
    const std::string& ak,
    std::unique_ptr<base::Value> av,
    const std::string& bk,
    std::unique_ptr<base::Value> bv,
    const std::string& ck,
    std::unique_ptr<base::Value> cv);

}  // namespace test_util
}  // namespace json_schema_compiler

#endif  // TOOLS_JSON_SCHEMA_COMPILER_TEST_TEST_UTIL_H_
