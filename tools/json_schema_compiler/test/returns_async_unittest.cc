// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/returns_async.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

// Tests that the returns async schema format behaves similar to how a callback
// parameter would. See callbacks_unittest.cc for the standard callback
// behavior.
TEST(JsonSchemaCompilerReturnsAsyncTest, ReturnsObjectResultCreate) {
  test::api::returns_async::SupportsPromises::Results::SomeObject some_object;
  some_object.state = test::api::returns_async::ENUMERATION_FOO;
  base::Value results = base::Value::FromUniquePtrValue(
      test::api::returns_async::SupportsPromises::Results::Create(some_object));

  base::Value expected_dict = base::Value(base::Value::Type::DICTIONARY);
  expected_dict.SetKey("state", base::Value("foo"));
  base::Value expected = base::Value(base::Value::Type::LIST);
  expected.Append(std::move(expected_dict));
  EXPECT_EQ(expected, results);
}
