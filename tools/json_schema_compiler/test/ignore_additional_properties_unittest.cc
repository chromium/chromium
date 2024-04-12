// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/ignore_additional_properties.h"

#include <memory>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace iap = test::api::ignore_additional_properties;

TEST(JsonSchemaCompilerIgnoreAdditionalPropertiesTest, TypePopulate) {
  {
    // Including the required property and additional properties should succeed.
    base::Value::List list_value;
    list_value.Append("asdf");
    list_value.Append(4);
    base::Value::Dict type_value;
    type_value.Set("string", "value");
    type_value.Set("other", 9);
    type_value.Set("another", std::move(list_value));
    auto type = iap::IgnoreAdditionalPropertiesType::FromValue(type_value);
    ASSERT_TRUE(type);
    EXPECT_EQ(type->string, "value");
  }
  {
    // Just including the required property should succeed.
    base::Value::Dict type_value;
    type_value.Set("string", "value");
    auto type = iap::IgnoreAdditionalPropertiesType::FromValue(type_value);
    ASSERT_TRUE(type);
    EXPECT_EQ(type->string, "value");
  }
  {
    // Including additional properties but not the required property should
    // fail.
    base::Value::Dict type_value;
    type_value.Set("other", 9);
    auto type = iap::IgnoreAdditionalPropertiesType::FromValue(type_value);
    ASSERT_FALSE(type);
  }
  {
    // Including the required property, but using the wrong type should fail.
    base::Value::Dict type_dict;
    type_dict.Set("string", 3);
    auto type = iap::IgnoreAdditionalPropertiesType::FromValue(type_dict);
    EXPECT_FALSE(type);
  }
}

TEST(JsonSchemaCompilerIgnoreAdditionalPropertiesTest, ParamsCreate) {
  base::Value::Dict param_object_dict;
  param_object_dict.Set("str", "a");
  param_object_dict.Set("num", 1);
  base::Value param_object_value(std::move(param_object_dict));
  base::Value::List params_value;
  params_value.Append(param_object_value.Clone());
  auto params(
      iap::IgnoreAdditionalPropertiesParams::Params::Create(params_value));
  EXPECT_TRUE(params.has_value());
}
