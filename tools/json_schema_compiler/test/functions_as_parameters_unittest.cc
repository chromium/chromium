// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/functions_as_parameters.h"

#include <memory>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

using test::api::functions_as_parameters::FunctionType;
using test::api::functions_as_parameters::OptionalFunctionType;

TEST(JsonSchemaCompilerFunctionsAsParametersTest, PopulateRequiredFunction) {
  // The expectation is that if any value is set for the function, then
  // the function is "present".
  {
    base::DictionaryValue empty_value;
    FunctionType out;
    EXPECT_FALSE(FunctionType::Populate(empty_value, &out));
  }
  {
    base::DictionaryValue value;
    base::DictionaryValue function_dict;
    value.SetKey("event_callback", function_dict.Clone());
    FunctionType out;
    ASSERT_TRUE(FunctionType::Populate(value, &out));
    EXPECT_TRUE(out.event_callback.empty());
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, RequiredFunctionToValue) {
  {
    base::DictionaryValue value;
    base::DictionaryValue function_dict;
    value.SetKey("event_callback", function_dict.Clone());

    FunctionType out;
    ASSERT_TRUE(FunctionType::Populate(value, &out));
    EXPECT_TRUE(value.Equals(out.ToValue().get()));
  }
  {
    base::DictionaryValue value;
    base::DictionaryValue expected_value;
    base::DictionaryValue function_dict;
    value.SetKey("event_callback", function_dict.Clone());
    expected_value.SetKey("event_callback", function_dict.Clone());

    FunctionType out;
    ASSERT_TRUE(FunctionType::Populate(value, &out));
    EXPECT_TRUE(expected_value.Equals(out.ToValue().get()));
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, PopulateOptionalFunction) {
  {
    base::DictionaryValue empty_value;
    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(empty_value, &out));
    EXPECT_FALSE(out.event_callback.get());
  }
  {
    base::DictionaryValue value;
    base::DictionaryValue function_value;
    value.SetKey("event_callback", function_value.Clone());
    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(value, &out));
    EXPECT_TRUE(out.event_callback.get());
  }
  {
    base::DictionaryValue value;
    base::DictionaryValue function_value;
    value.SetKey("event_callback", function_value.Clone());
    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(value, &out));
    EXPECT_TRUE(out.event_callback.get());
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, OptionalFunctionToValue) {
  {
    base::DictionaryValue empty_value;
    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(empty_value, &out));
    // event_callback should not be set in the return from ToValue.
    EXPECT_TRUE(empty_value.Equals(out.ToValue().get()));
  }
  {
    base::DictionaryValue value;
    base::DictionaryValue function_value;
    value.SetKey("event_callback", function_value.Clone());

    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(value, &out));
    EXPECT_TRUE(value.Equals(out.ToValue().get()));
  }
}
