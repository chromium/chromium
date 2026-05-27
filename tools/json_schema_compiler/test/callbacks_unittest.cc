// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/callbacks.h"

#include <memory>
#include <utility>

#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

TEST(JsonSchemaCompilerCallbacksTest, ReturnsObjectResultCreate) {
  test::api::callbacks::ReturnsObject::Results::SomeObject some_object;
  some_object.state = test::api::callbacks::Enumeration::kFoo;
  base::Value results(
      test::api::callbacks::ReturnsObject::Results::Create(some_object));

  base::DictValue expected_dict;
  expected_dict.Set("state", "foo");
  base::ListValue expected;
  expected.Append(std::move(expected_dict));
  EXPECT_EQ(expected, results);
}

TEST(JsonSchemaCompilerCallbacksTest, ReturnsMultipleResultCreate) {
  test::api::callbacks::ReturnsMultiple::Results::SomeObject some_object;
  some_object.state = test::api::callbacks::Enumeration::kFoo;
  base::Value results(
      test::api::callbacks::ReturnsMultiple::Results::Create(5, some_object));

  base::DictValue expected_dict;
  expected_dict.Set("state", "foo");
  base::ListValue expected;
  expected.Append(5);
  expected.Append(std::move(expected_dict));
  EXPECT_EQ(expected, results);
}
