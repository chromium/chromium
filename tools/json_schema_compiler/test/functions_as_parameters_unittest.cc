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

TEST(JsonSchemaCompilerFunctionsAsParametersTest, PopulateRequiredFunction) {
  // The expectation is that if any value is set for the function, then
  // the function is "present".
  {
    base::Value empty_value;
    FunctionType out;
    EXPECT_FALSE(FunctionType::Populate(empty_value, out));
  }
  {
    base::Value value(base::Value::Type::DICT);
    value.GetDict().Set("event_callback", base::Value::Dict());

    FunctionType out;
    ASSERT_TRUE(FunctionType::Populate(value, out));
    EXPECT_TRUE(out.event_callback.empty());
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, RequiredFunctionToValue) {
  {
    base::Value::Dict value;
    value.Set("event_callback", base::Value::Dict());

    FunctionType out;
    ASSERT_TRUE(FunctionType::Populate(value, out));
    EXPECT_EQ(value, out.ToValue());
  }
  {
    base::Value::Dict value;
    base::Value::Dict expected_value;
    value.Set("event_callback", base::Value::Dict());
    expected_value.Set("event_callback", base::Value::Dict());

    FunctionType out;
    ASSERT_TRUE(FunctionType::Populate(value, out));
    EXPECT_EQ(expected_value, out.ToValue());
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, PopulateOptionalFunction) {
  {
    base::Value::Dict empty_dictionary;
    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(empty_dictionary, out));
    EXPECT_FALSE(out.event_callback.has_value());
  }
  {
    base::Value::Dict value;
    value.Set("event_callback", base::Value::Dict());

    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(value, out));
    EXPECT_TRUE(out.event_callback.has_value());
  }
  {
    base::Value::Dict value;
    value.Set("event_callback", base::Value::Dict());

    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(value, out));
    EXPECT_TRUE(out.event_callback.has_value());
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, OptionalFunctionToValue) {
  {
    base::Value::Dict empty_value;
    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(empty_value, out));
    // event_callback should not be set in the return from ToValue.
    EXPECT_EQ(empty_value, out.ToValue());
  }
  {
    base::Value::Dict value;
    value.Set("event_callback", base::Value::Dict());

    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(value, out));
    EXPECT_EQ(value, out.ToValue());
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, SerializableFunctionTypes) {
  constexpr char kFunction[] = "function() {}";
  SerializableFunctionType serializable_type;
  serializable_type.function_property = kFunction;
  base::Value::Dict serialized = serializable_type.ToValue();
  SerializableFunctionType deserialized;
  ASSERT_TRUE(
      SerializableFunctionType::Populate(std::move(serialized), deserialized));
  EXPECT_EQ(kFunction, serializable_type.function_property);
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest,
     OptionalSerializableFunctionTypes) {
  constexpr char kFunction[] = "function() {}";
  {
    // Test with the optional property set.
    OptionalSerializableFunctionType serializable_type;
    serializable_type.function_property = kFunction;
    base::Value::Dict serialized = serializable_type.ToValue();
    OptionalSerializableFunctionType deserialized;
    ASSERT_TRUE(OptionalSerializableFunctionType::Populate(
        std::move(serialized), deserialized));
    ASSERT_TRUE(serializable_type.function_property);
    EXPECT_EQ(kFunction, *serializable_type.function_property);
  }
  {
    // Test without the property set.
    OptionalSerializableFunctionType serializable_type;
    base::Value::Dict serialized = serializable_type.ToValue();
    OptionalSerializableFunctionType deserialized;
    ASSERT_TRUE(OptionalSerializableFunctionType::Populate(
        std::move(serialized), deserialized));
    EXPECT_FALSE(serializable_type.function_property);
  }
}
