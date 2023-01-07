// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/callbacks.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

TEST(JsonSchemaCompilerCallbacksTest, ReturnsObjectResultCreate) {
  test::api::callbacks::ReturnsObject::Results::SomeObject some_object;
  some_object.state = test::api::callbacks::ENUMERATION_FOO;
  base::Value results(
      test::api::callbacks::ReturnsObject::Results::Create(some_object));

  base::Value expected_dict(base::Value::Type::DICTIONARY);
  expected_dict.SetStringPath("state", "foo");
  base::Value expected(base::Value::Type::LIST);
  expected.Append(std::move(expected_dict));
  EXPECT_EQ(expected, results);
}

TEST(JsonSchemaCompilerCallbacksTest, ReturnsMultipleResultCreate) {
  test::api::callbacks::ReturnsMultiple::Results::SomeObject some_object;
  some_object.state = test::api::callbacks::ENUMERATION_FOO;
  base::Value results(
      test::api::callbacks::ReturnsMultiple::Results::Create(5, some_object));

  base::Value expected_dict(base::Value::Type::DICTIONARY);
  expected_dict.SetStringPath("state", "foo");
  base::Value expected(base::Value::Type::LIST);
  expected.Append(5);
  expected.Append(std::move(expected_dict));
  EXPECT_EQ(expected, results);
}
