// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/test_util.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"

namespace json_schema_compiler {
namespace test_util {

base::Value ReadJson(std::string_view json) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      json, base::JSON_ALLOW_TRAILING_COMMAS);
  // CHECK not ASSERT since passing invalid |json| is a test error.
  CHECK(parsed_json.has_value()) << parsed_json.error().message;
  return std::move(*parsed_json);
}

base::Value List(base::Value a) {
  base::Value::List list;
  list.Append(std::move(a));
  return base::Value(std::move(list));
}
base::Value List(base::Value a, base::Value b) {
  base::Value::List list;
  list.Append(std::move(a));
  list.Append(std::move(b));
  return base::Value(std::move(list));
}
base::Value List(base::Value a, base::Value b, base::Value c) {
  base::Value::List list;
  list.Append(std::move(a));
  list.Append(std::move(b));
  list.Append(std::move(c));
  return base::Value(std::move(list));
}

base::Value Dictionary(const std::string& ak, base::Value av) {
  base::Value::Dict dict;
  dict.Set(ak, std::move(av));
  return base::Value(std::move(dict));
}
base::Value Dictionary(const std::string& ak,
                       base::Value av,
                       const std::string& bk,
                       base::Value bv) {
  base::Value::Dict dict;
  dict.Set(ak, std::move(av));
  dict.Set(bk, std::move(bv));
  return base::Value(std::move(dict));
}
base::Value Dictionary(const std::string& ak,
                       base::Value av,
                       const std::string& bk,
                       base::Value bv,
                       const std::string& ck,
                       base::Value cv) {
  base::Value::Dict dict;
  dict.Set(ak, std::move(av));
  dict.Set(bk, std::move(bv));
  dict.Set(ck, std::move(cv));
  return base::Value(std::move(dict));
}

}  // namespace test_util
}  // namespace json_schema_compiler
