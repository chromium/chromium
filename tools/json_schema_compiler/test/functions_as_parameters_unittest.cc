// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/functions_as_parameters.h"

#include <memory>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/test_util.h"

using test::api::functions_as_parameters::FunctionType;
using test::api::functions_as_parameters::OptionalFunctionType;
using test::api::functions_as_parameters::OptionalSerializableFunctionType;
using test::api::functions_as_parameters::SerializableFunctionType;

TEST(JsonSchemaCompilerFunctionsAsParametersTest, RequiredFunctionFromValue) {
  // The expectation is that if any value is set for the function, then
  // the function is "present".
  {
    base::Value empty_value;
    EXPECT_FALSE(FunctionType::FromValue(empty_value));
  }
  {
    base::Value value(base::Value::Type::DICT);
    value.GetDict().Set("event_callback", base::Value::Dict());

    auto out = FunctionType::FromValue(value);
    ASSERT_TRUE(out);
    EXPECT_TRUE(out->event_callback.empty());
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, RequiredFunctionToValue) {
  {
    base::Value::Dict value;
    value.Set("event_callback", base::Value::Dict());

    auto out = FunctionType::FromValue(value);
    ASSERT_TRUE(out);
    EXPECT_EQ(value, out->ToValue());
  }
  {
    base::Value::Dict value;
    base::Value::Dict expected_value;
    value.Set("event_callback", base::Value::Dict());
    expected_value.Set("event_callback", base::Value::Dict());

    auto out = FunctionType::FromValue(value);
    ASSERT_TRUE(out);
    EXPECT_EQ(expected_value, out->ToValue());
    EXPECT_EQ(out->Clone().ToValue(), out->ToValue());
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, OptionalFunctionFromValue) {
  {
    base::Value::Dict empty_dictionary;
    auto out = OptionalFunctionType::FromValue(empty_dictionary);
    ASSERT_TRUE(out);
    EXPECT_FALSE(out->event_callback.has_value());
    EXPECT_EQ(out->Clone().ToValue(), out->ToValue());
  }
  {
    base::Value::Dict value;
    value.Set("event_callback", base::Value::Dict());

    auto out = OptionalFunctionType::FromValue(value);
    ASSERT_TRUE(out);
    EXPECT_TRUE(out->event_callback.has_value());
    EXPECT_EQ(out->Clone().ToValue(), out->ToValue());
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, OptionalFunctionToValue) {
  {
    base::Value::Dict empty_value;
    auto out = OptionalFunctionType::FromValue(empty_value);
    ASSERT_TRUE(out);
    // event_callback should not be set in the return from ToValue.
    EXPECT_EQ(empty_value, out->ToValue());
  }
  {
    base::Value::Dict value;
    value.Set("event_callback", base::Value::Dict());

    auto out = OptionalFunctionType::FromValue(value);
    ASSERT_TRUE(out);
    EXPECT_EQ(value, out->ToValue());
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, SerializableFunctionTypes) {
  constexpr char kFunction[] = "function() {}";
  SerializableFunctionType serializable_type;
  serializable_type.function_property = kFunction;
  base::Value::Dict serialized = serializable_type.ToValue();
  ASSERT_TRUE(SerializableFunctionType::FromValue(std::move(serialized)));
  EXPECT_EQ(kFunction, serializable_type.function_property);
  EXPECT_EQ(serializable_type.Clone().ToValue(), serializable_type.ToValue());
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest,
     OptionalSerializableFunctionTypes) {
  constexpr char kFunction[] = "function() {}";
  {
    // Test with the optional property set.
    OptionalSerializableFunctionType serializable_type;
    serializable_type.function_property = kFunction;
    base::Value::Dict serialized = serializable_type.ToValue();
    ASSERT_TRUE(
        OptionalSerializableFunctionType::FromValue(std::move(serialized)));
    ASSERT_TRUE(serializable_type.function_property);
    EXPECT_EQ(kFunction, *serializable_type.function_property);
    EXPECT_EQ(serializable_type.Clone().ToValue(), serializable_type.ToValue());
  }
  {
    // Test without the property set.
    OptionalSerializableFunctionType serializable_type;
    base::Value::Dict serialized = serializable_type.ToValue();
    ASSERT_TRUE(
        OptionalSerializableFunctionType::FromValue(std::move(serialized)));
    EXPECT_FALSE(serializable_type.function_property);
    EXPECT_EQ(serializable_type.Clone().ToValue(), serializable_type.ToValue());
  }
}
