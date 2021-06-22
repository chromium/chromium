// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/additional_properties.h"

#include <memory>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace ap = test::api::additional_properties;

TEST(JsonSchemaCompilerAdditionalPropertiesTest,
    AdditionalPropertiesTypePopulate) {
  {
    base::ListValue list_value;
    list_value.AppendString("asdf");
    list_value.AppendInteger(4);
    auto type_value = std::make_unique<base::DictionaryValue>();
    type_value->SetString("string", "value");
    type_value->SetInteger("other", 9);
    type_value->SetKey("another", std::move(list_value));
    auto type = std::make_unique<ap::AdditionalPropertiesType>();
    ASSERT_TRUE(
        ap::AdditionalPropertiesType::Populate(*type_value, type.get()));
    EXPECT_TRUE(type->additional_properties.Equals(type_value.get()));
  }
  {
    auto type_value = std::make_unique<base::DictionaryValue>();
    type_value->SetInteger("string", 3);
    auto type = std::make_unique<ap::AdditionalPropertiesType>();
    EXPECT_FALSE(
        ap::AdditionalPropertiesType::Populate(*type_value, type.get()));
  }
}

TEST(JsonSchemaCompilerAdditionalPropertiesTest,
    AdditionalPropertiesParamsCreate) {
  auto param_object_value = std::make_unique<base::DictionaryValue>();
  param_object_value->SetString("str", "a");
  param_object_value->SetInteger("num", 1);
  auto params_value = std::make_unique<base::ListValue>();
  params_value->Append(param_object_value->CreateDeepCopy());
  std::unique_ptr<ap::AdditionalProperties::Params> params(
      ap::AdditionalProperties::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  EXPECT_TRUE(params->param_object.additional_properties.Equals(
      param_object_value.get()));
}

TEST(JsonSchemaCompilerAdditionalPropertiesTest,
    ReturnAdditionalPropertiesResultCreate) {
  ap::ReturnAdditionalProperties::Results::ResultObject result_object;
  result_object.integer = 5;
  result_object.additional_properties["key"] = "value";

  std::vector<base::Value> expected;
  {
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetIntKey("integer", 5);
    dict.SetStringKey("key", "value");
    expected.push_back(std::move(dict));
  }

  EXPECT_EQ(expected,
            ap::ReturnAdditionalProperties::Results::Create(result_object));
}
