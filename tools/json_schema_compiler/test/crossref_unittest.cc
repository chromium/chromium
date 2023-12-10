// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/crossref.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/simple_api.h"

namespace crossref = test::api::crossref;
namespace simple_api = test::api::simple_api;

namespace {

base::Value::Dict CreateTestTypeValue() {
  base::Value::Dict dict;
  dict.Set("number", 1.1);
  dict.Set("integer", 4);
  dict.Set("string", "bling");
  dict.Set("boolean", true);
  return dict;
}

}  // namespace

TEST(JsonSchemaCompilerCrossrefTest, CrossrefTypePopulateAndToValue) {
  base::Value::Dict crossref_orig;
  crossref_orig.Set("testType", CreateTestTypeValue());
  crossref_orig.Set("testEnumRequired", "one");
  crossref_orig.Set("testEnumOptional", "two");

  // Test Populate of the value --> compiled type.
  auto crossref_type = crossref::CrossrefType::FromValue(crossref_orig);
  ASSERT_TRUE(crossref_type);
  EXPECT_EQ(1.1, crossref_type->test_type.number);
  EXPECT_EQ(4, crossref_type->test_type.integer);
  EXPECT_EQ("bling", crossref_type->test_type.string);
  EXPECT_EQ(true, crossref_type->test_type.boolean);
  EXPECT_EQ(simple_api::TestEnum::kOne, crossref_type->test_enum_required);
  EXPECT_EQ(simple_api::TestEnum::kTwo, crossref_type->test_enum_optional);
  EXPECT_EQ(simple_api::TestEnum::kNone,
            crossref_type->test_enum_optional_extra);

  // Test ToValue of the compiled type --> value.
  base::Value::Dict crossref_value = crossref_type->ToValue();
  EXPECT_EQ(crossref_orig, crossref_value);

  EXPECT_EQ(crossref_type->Clone().ToValue(), crossref_type->ToValue());
}

TEST(JsonSchemaCompilerCrossrefTest, TestTypeOptionalParamCreate) {
  base::Value::List params_value;
  params_value.Append(CreateTestTypeValue());
  std::optional<crossref::TestTypeOptionalParam::Params> params(
      crossref::TestTypeOptionalParam::Params::Create(params_value));
  EXPECT_TRUE(params.has_value());
  EXPECT_TRUE(params->test_type);
  EXPECT_EQ(CreateTestTypeValue(), params->test_type->ToValue());
}

TEST(JsonSchemaCompilerCrossrefTest, TestTypeOptionalParamFail) {
  base::Value::List params_value;
  base::Value::Dict test_type_value = CreateTestTypeValue();
  test_type_value.Remove("number");
  params_value.Append(std::move(test_type_value));
  std::optional<crossref::TestTypeOptionalParam::Params> params(
      crossref::TestTypeOptionalParam::Params::Create(params_value));
  EXPECT_FALSE(params.has_value());
}

TEST(JsonSchemaCompilerCrossrefTest, GetTestType) {
  base::Value::Dict value = CreateTestTypeValue();
  auto test_type = simple_api::TestType::FromValue(value);
  ASSERT_TRUE(test_type);

  base::Value::List results =
      crossref::GetTestType::Results::Create(*test_type);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(value, results[0]);
}

TEST(JsonSchemaCompilerCrossrefTest, TestTypeInObjectParamsCreate) {
  {
    base::Value::List params_value;
    base::Value::Dict param_object_value;
    param_object_value.Set("testType", CreateTestTypeValue());
    param_object_value.Set("boolean", true);
    params_value.Append(std::move(param_object_value));
    std::optional<crossref::TestTypeInObject::Params> params(
        crossref::TestTypeInObject::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_TRUE(params->param_object.test_type);
    EXPECT_TRUE(params->param_object.boolean);
    EXPECT_EQ(CreateTestTypeValue(), params->param_object.test_type->ToValue());
  }
  {
    base::Value::List params_value;
    base::Value::Dict param_object_value;
    param_object_value.Set("boolean", true);
    params_value.Append(std::move(param_object_value));
    std::optional<crossref::TestTypeInObject::Params> params(
        crossref::TestTypeInObject::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_FALSE(params->param_object.test_type);
    EXPECT_TRUE(params->param_object.boolean);
  }
  {
    base::Value::List params_value;
    base::Value::Dict param_object_value;
    param_object_value.Set("testType", "invalid");
    param_object_value.Set("boolean", true);
    params_value.Append(std::move(param_object_value));
    std::optional<crossref::TestTypeInObject::Params> params(
        crossref::TestTypeInObject::Params::Create(params_value));
    EXPECT_FALSE(params.has_value());
  }
  {
    base::Value::List params_value;
    base::Value::Dict param_object_value;
    param_object_value.Set("testType", CreateTestTypeValue());
    params_value.Append(std::move(param_object_value));
    std::optional<crossref::TestTypeInObject::Params> params(
        crossref::TestTypeInObject::Params::Create(params_value));
    EXPECT_FALSE(params.has_value());
  }
}
