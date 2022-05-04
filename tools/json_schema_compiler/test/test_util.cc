// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/test_util.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"

namespace json_schema_compiler {
namespace test_util {

base::Value ReadJson(const base::StringPiece& json) {
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(
          json, base::JSON_ALLOW_TRAILING_COMMAS);
  // CHECK not ASSERT since passing invalid |json| is a test error.
  CHECK(parsed_json.value) << parsed_json.error_message;
  return std::move(*parsed_json.value);
}

std::unique_ptr<base::ListValue> List(std::unique_ptr<base::Value> a) {
  auto list = std::make_unique<base::ListValue>();
  list->GetList().Append(base::Value::FromUniquePtrValue(std::move(a)));
  return list;
}
std::unique_ptr<base::ListValue> List(std::unique_ptr<base::Value> a,
                                      std::unique_ptr<base::Value> b) {
  auto list = std::make_unique<base::ListValue>();
  list->GetList().Append(base::Value::FromUniquePtrValue(std::move(a)));
  list->GetList().Append(base::Value::FromUniquePtrValue(std::move(b)));
  return list;
}
std::unique_ptr<base::ListValue> List(std::unique_ptr<base::Value> a,
                                      std::unique_ptr<base::Value> b,
                                      std::unique_ptr<base::Value> c) {
  auto list = std::make_unique<base::ListValue>();
  list->GetList().Append(base::Value::FromUniquePtrValue(std::move(a)));
  list->GetList().Append(base::Value::FromUniquePtrValue(std::move(b)));
  list->GetList().Append(base::Value::FromUniquePtrValue(std::move(c)));
  return list;
}

std::unique_ptr<base::DictionaryValue> Dictionary(
    const std::string& ak,
    std::unique_ptr<base::Value> av) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetKey(ak, base::Value::FromUniquePtrValue(std::move(av)));
  return dict;
}
std::unique_ptr<base::DictionaryValue> Dictionary(
    const std::string& ak,
    std::unique_ptr<base::Value> av,
    const std::string& bk,
    std::unique_ptr<base::Value> bv) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetKey(ak, base::Value::FromUniquePtrValue(std::move(av)));
  dict->SetKey(bk, base::Value::FromUniquePtrValue(std::move(bv)));
  return dict;
}
std::unique_ptr<base::DictionaryValue> Dictionary(
    const std::string& ak,
    std::unique_ptr<base::Value> av,
    const std::string& bk,
    std::unique_ptr<base::Value> bv,
    const std::string& ck,
    std::unique_ptr<base::Value> cv) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetKey(ak, base::Value::FromUniquePtrValue(std::move(av)));
  dict->SetKey(bk, base::Value::FromUniquePtrValue(std::move(bv)));
  dict->SetKey(ck, base::Value::FromUniquePtrValue(std::move(cv)));
  return dict;
}

}  // namespace test_util
}  // namespace json_schema_compiler
