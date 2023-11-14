// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/any.h"

TEST(JsonSchemaCompilerAnyTest, PopulateAndClone) {
  {
    base::Value::Dict any_type_dict;
    any_type_dict.Set("any", "value");
    auto any_type = test::api::any::AnyType::FromValue(any_type_dict);
    ASSERT_TRUE(any_type);
    base::Value::Dict any_type_to_value(any_type->ToValue());
    EXPECT_EQ(any_type_dict, any_type_to_value);

    test::api::any::AnyType any_type_copy = any_type->Clone();
    EXPECT_EQ(any_type_dict, any_type_copy.ToValue());
  }
  {
    base::Value::Dict any_type_dict;
    any_type_dict.Set("any", 5);
    auto any_type = test::api::any::AnyType::FromValue(any_type_dict);
    ASSERT_TRUE(any_type);
    base::Value::Dict any_type_to_value(any_type->ToValue());
    EXPECT_EQ(any_type_dict, any_type_to_value);

    test::api::any::AnyType any_type_copy = any_type->Clone();
    EXPECT_EQ(any_type_dict, any_type_copy.ToValue());
  }
}

TEST(JsonSchemaCompilerAnyTest, OptionalAnyParamsCreate) {
  {
    base::Value::List params_value;
    std::optional<test::api::any::OptionalAny::Params> params(
        test::api::any::OptionalAny::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_FALSE(params->any_name);
  }
  {
    base::Value::List params_value;
    base::Value param("asdf");
    params_value.Append(param.Clone());
    std::optional<test::api::any::OptionalAny::Params> params(
        test::api::any::OptionalAny::Params::Create(params_value));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->any_name);
    EXPECT_EQ(*params->any_name, param);
  }
  {
    base::Value::List params_value;
    base::Value param(true);
    params_value.Append(param.Clone());
    std::optional<test::api::any::OptionalAny::Params> params(
        test::api::any::OptionalAny::Params::Create(params_value));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->any_name);
    EXPECT_EQ(*params->any_name, param);
  }
}
