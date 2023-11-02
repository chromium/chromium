// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/returns_async.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

// Tests that the returns async schema format behaves similar to how a callback
// parameter would. See callbacks_unittest.cc for the standard callback
// behavior.
TEST(JsonSchemaCompilerReturnsAsyncTest, ReturnsObjectResultCreate) {
  test::api::returns_async::SupportsPromises::Results::SomeObject some_object;
  some_object.state = test::api::returns_async::Enumeration::kFoo;
  base::Value results(
      test::api::returns_async::SupportsPromises::Results::Create(some_object));

  base::Value::Dict expected_dict;
  expected_dict.Set("state", "foo");
  base::Value::List expected;
  expected.Append(std::move(expected_dict));
  EXPECT_EQ(expected, results);
}
