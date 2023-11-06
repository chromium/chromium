// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/callbacks.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

TEST(JsonSchemaCompilerCallbacksTest, ReturnsObjectResultCreate) {
  test::api::callbacks::ReturnsObject::Results::SomeObject some_object;
  some_object.state = test::api::callbacks::Enumeration::kFoo;
  base::Value results(
      test::api::callbacks::ReturnsObject::Results::Create(some_object));

  base::Value::Dict expected_dict;
  expected_dict.Set("state", "foo");
  base::Value::List expected;
  expected.Append(std::move(expected_dict));
  EXPECT_EQ(expected, results);
}

TEST(JsonSchemaCompilerCallbacksTest, ReturnsMultipleResultCreate) {
  test::api::callbacks::ReturnsMultiple::Results::SomeObject some_object;
  some_object.state = test::api::callbacks::Enumeration::kFoo;
  base::Value results(
      test::api::callbacks::ReturnsMultiple::Results::Create(5, some_object));

  base::Value::Dict expected_dict;
  expected_dict.Set("state", "foo");
  base::Value::List expected;
  expected.Append(5);
  expected.Append(std::move(expected_dict));
  EXPECT_EQ(expected, results);
}
