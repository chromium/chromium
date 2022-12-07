// Copyright 2012 The Chromium Authors
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
    base::Value::List list_value;
    list_value.Append("asdf");
    list_value.Append(4);
    base::Value::Dict type_value;
    type_value.Set("string", "value");
    type_value.Set("other", 9);
    type_value.Set("another", std::move(list_value));
    auto type = std::make_unique<ap::AdditionalPropertiesType>();
    ASSERT_TRUE(ap::AdditionalPropertiesType::Populate(
        base::Value(type_value.Clone()), type.get()));
    EXPECT_EQ(type->additional_properties, type_value);
  }
  {
    base::Value::Dict type_dict;
    type_dict.Set("string", 3);
    base::Value type_value(std::move(type_dict));
    auto type = std::make_unique<ap::AdditionalPropertiesType>();
    EXPECT_FALSE(ap::AdditionalPropertiesType::Populate(
        base::Value(type_value.Clone()), type.get()));
  }
}

TEST(JsonSchemaCompilerAdditionalPropertiesTest,
    AdditionalPropertiesParamsCreate) {
  base::Value::Dict param_object_dict;
  param_object_dict.Set("str", "a");
  param_object_dict.Set("num", 1);
  base::Value param_object_value(std::move(param_object_dict));
  base::Value::List params_value;
  params_value.Append(param_object_value.Clone());
  std::unique_ptr<ap::AdditionalProperties::Params> params(
      ap::AdditionalProperties::Params::Create(params_value));
  EXPECT_TRUE(params.get());
  EXPECT_EQ(params->param_object.additional_properties, param_object_value);
}

TEST(JsonSchemaCompilerAdditionalPropertiesTest,
    ReturnAdditionalPropertiesResultCreate) {
  ap::ReturnAdditionalProperties::Results::ResultObject result_object;
  result_object.integer = 5;
  result_object.additional_properties["key"] = "value";

  base::Value::List expected;
  {
    base::Value::Dict dict;
    dict.Set("integer", 5);
    dict.Set("key", "value");
    expected.Append(std::move(dict));
  }

  EXPECT_EQ(expected,
            ap::ReturnAdditionalProperties::Results::Create(result_object));
}
