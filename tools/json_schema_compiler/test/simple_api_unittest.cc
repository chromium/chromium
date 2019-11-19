// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/simple_api.h"

#include <memory>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace simple_api = test::api::simple_api;

namespace {

static std::unique_ptr<base::DictionaryValue> CreateTestTypeDictionary() {
  auto value = std::make_unique<base::DictionaryValue>();
  value->SetKey("number", base::Value(1.1));
  value->SetKey("integer", base::Value(4));
  value->SetKey("string", base::Value("bling"));
  value->SetKey("boolean", base::Value(true));
  return value;
}

}  // namespace

TEST(JsonSchemaCompilerSimpleTest, IncrementIntegerResultCreate) {
  std::unique_ptr<base::ListValue> results =
      simple_api::IncrementInteger::Results::Create(5);
  base::ListValue expected;
  expected.AppendInteger(5);
  EXPECT_TRUE(results->Equals(&expected));
}

TEST(JsonSchemaCompilerSimpleTest, IncrementIntegerParamsCreate) {
  auto params_value = std::make_unique<base::ListValue>();
  params_value->AppendInteger(6);
  std::unique_ptr<simple_api::IncrementInteger::Params> params(
      simple_api::IncrementInteger::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  EXPECT_EQ(6, params->num);
}

TEST(JsonSchemaCompilerSimpleTest, NumberOfParams) {
  {
    auto params_value = std::make_unique<base::ListValue>();
    params_value->AppendString("text");
    params_value->AppendString("text");
    std::unique_ptr<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
  {
    auto params_value = std::make_unique<base::ListValue>();
    std::unique_ptr<simple_api::IncrementInteger::Params> params(
        simple_api::IncrementInteger::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalStringParamsCreate) {
  {
    auto params_value = std::make_unique<base::ListValue>();
    std::unique_ptr<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_FALSE(params->str.get());
  }
  {
    auto params_value = std::make_unique<base::ListValue>();
    params_value->AppendString("asdf");
    std::unique_ptr<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_TRUE(params->str.get());
    EXPECT_EQ("asdf", *params->str);
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalParamsTakingNull) {
  {
    auto params_value = std::make_unique<base::ListValue>();
    params_value->Append(std::make_unique<base::Value>());
    std::unique_ptr<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_FALSE(params->str.get());
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalStringParamsWrongType) {
  {
    auto params_value = std::make_unique<base::ListValue>();
    params_value->AppendInteger(5);
    std::unique_ptr<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalBeforeRequired) {
  {
    auto params_value = std::make_unique<base::ListValue>();
    params_value->Append(std::make_unique<base::Value>());
    params_value->AppendString("asdf");
    std::unique_ptr<simple_api::OptionalBeforeRequired::Params> params(
        simple_api::OptionalBeforeRequired::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_FALSE(params->first.get());
    EXPECT_EQ("asdf", params->second);
  }
}

TEST(JsonSchemaCompilerSimpleTest, NoParamsResultCreate) {
  std::unique_ptr<base::ListValue> results =
      simple_api::OptionalString::Results::Create();
  base::ListValue expected;
  EXPECT_TRUE(results->Equals(&expected));
}

TEST(JsonSchemaCompilerSimpleTest, TestTypePopulate) {
  {
    auto test_type = std::make_unique<simple_api::TestType>();
    std::unique_ptr<base::DictionaryValue> value = CreateTestTypeDictionary();
    EXPECT_TRUE(simple_api::TestType::Populate(*value, test_type.get()));
    EXPECT_EQ("bling", test_type->string);
    EXPECT_EQ(1.1, test_type->number);
    EXPECT_EQ(4, test_type->integer);
    EXPECT_EQ(true, test_type->boolean);
    EXPECT_TRUE(value->Equals(test_type->ToValue().get()));
  }
  {
    auto test_type = std::make_unique<simple_api::TestType>();
    std::unique_ptr<base::DictionaryValue> value = CreateTestTypeDictionary();
    value->Remove("number", NULL);
    EXPECT_FALSE(simple_api::TestType::Populate(*value, test_type.get()));
  }
}

TEST(JsonSchemaCompilerSimpleTest, GetTestType) {
  {
    std::unique_ptr<base::DictionaryValue> value = CreateTestTypeDictionary();
    auto test_type = std::make_unique<simple_api::TestType>();
    EXPECT_TRUE(simple_api::TestType::Populate(*value, test_type.get()));
    std::unique_ptr<base::ListValue> results =
        simple_api::GetTestType::Results::Create(*test_type);

    base::DictionaryValue* result = NULL;
    results->GetDictionary(0, &result);
    EXPECT_TRUE(result->Equals(value.get()));
  }
}

TEST(JsonSchemaCompilerSimpleTest, OnIntegerFiredCreate) {
  {
    std::unique_ptr<base::ListValue> results(
        simple_api::OnIntegerFired::Create(5));
    base::ListValue expected;
    expected.AppendInteger(5);
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerSimpleTest, OnStringFiredCreate) {
  {
    std::unique_ptr<base::ListValue> results(
        simple_api::OnStringFired::Create("yo dawg"));
    base::ListValue expected;
    expected.AppendString("yo dawg");
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerSimpleTest, OnTestTypeFiredCreate) {
  {
    simple_api::TestType some_test_type;
    std::unique_ptr<base::DictionaryValue> expected =
        CreateTestTypeDictionary();
    ASSERT_TRUE(expected->GetDouble("number", &some_test_type.number));
    ASSERT_TRUE(expected->GetString("string", &some_test_type.string));
    ASSERT_TRUE(expected->GetInteger("integer", &some_test_type.integer));
    ASSERT_TRUE(expected->GetBoolean("boolean", &some_test_type.boolean));

    std::unique_ptr<base::ListValue> results(
        simple_api::OnTestTypeFired::Create(some_test_type));
    base::DictionaryValue* result = NULL;
    results->GetDictionary(0, &result);
    EXPECT_TRUE(result->Equals(expected.get()));
  }
}
