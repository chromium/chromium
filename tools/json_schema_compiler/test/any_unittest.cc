// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/any.h"

TEST(JsonSchemaCompilerAnyTest, AnyTypePopulate) {
  {
    test::api::any::AnyType any_type;
    base::Value::Dict any_type_dict;
    any_type_dict.Set("any", "value");
    EXPECT_TRUE(test::api::any::AnyType::Populate(any_type_dict, any_type));
    base::Value::Dict any_type_to_value(any_type.ToValue());
    EXPECT_EQ(any_type_dict, any_type_to_value);
  }
  {
    test::api::any::AnyType any_type;
    base::Value::Dict any_type_dict;
    any_type_dict.Set("any", 5);
    EXPECT_TRUE(test::api::any::AnyType::Populate(any_type_dict, any_type));
    base::Value::Dict any_type_to_value(any_type.ToValue());
    EXPECT_EQ(any_type_dict, any_type_to_value);
  }
}

TEST(JsonSchemaCompilerAnyTest, OptionalAnyParamsCreate) {
  {
    base::Value::List params_value;
    absl::optional<test::api::any::OptionalAny::Params> params(
        test::api::any::OptionalAny::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_FALSE(params->any_name);
  }
  {
    base::Value::List params_value;
    base::Value param("asdf");
    params_value.Append(param.Clone());
    absl::optional<test::api::any::OptionalAny::Params> params(
        test::api::any::OptionalAny::Params::Create(params_value));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->any_name);
    EXPECT_EQ(*params->any_name, param);
  }
  {
    base::Value::List params_value;
    base::Value param(true);
    params_value.Append(param.Clone());
    absl::optional<test::api::any::OptionalAny::Params> params(
        test::api::any::OptionalAny::Params::Create(params_value));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->any_name);
    EXPECT_EQ(*params->any_name, param);
  }
}
