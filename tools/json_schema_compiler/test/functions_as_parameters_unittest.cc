// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/functions_as_parameters.h"

#include <memory>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

using test::api::functions_as_parameters::FunctionType;
using test::api::functions_as_parameters::OptionalFunctionType;
using test::api::functions_as_parameters::OptionalSerializableFunctionType;
using test::api::functions_as_parameters::SerializableFunctionType;

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

TEST(JsonSchemaCompilerFunctionsAsParametersTest, SerializableFunctionTypes) {
  constexpr char kFunction[] = "function() {}";
  SerializableFunctionType serializable_type;
  serializable_type.function_property = kFunction;
  std::unique_ptr<base::DictionaryValue> serialized =
      serializable_type.ToValue();
  ASSERT_TRUE(serialized);
  SerializableFunctionType deserialized;
  ASSERT_TRUE(SerializableFunctionType::Populate(*serialized, &deserialized));
  EXPECT_EQ(kFunction, serializable_type.function_property);
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest,
     OptionalSerializableFunctionTypes) {
  constexpr char kFunction[] = "function() {}";
  {
    // Test with the optional property set.
    OptionalSerializableFunctionType serializable_type;
    serializable_type.function_property =
        std::make_unique<std::string>(kFunction);
    std::unique_ptr<base::DictionaryValue> serialized =
        serializable_type.ToValue();
    ASSERT_TRUE(serialized);
    OptionalSerializableFunctionType deserialized;
    ASSERT_TRUE(
        OptionalSerializableFunctionType::Populate(*serialized, &deserialized));
    ASSERT_TRUE(serializable_type.function_property);
    EXPECT_EQ(kFunction, *serializable_type.function_property);
  }
  {
    // Test without the property set.
    OptionalSerializableFunctionType serializable_type;
    std::unique_ptr<base::DictionaryValue> serialized =
        serializable_type.ToValue();
    ASSERT_TRUE(serialized);
    OptionalSerializableFunctionType deserialized;
    ASSERT_TRUE(
        OptionalSerializableFunctionType::Populate(*serialized, &deserialized));
    EXPECT_FALSE(serializable_type.function_property);
  }
}
