// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/functions_on_types.h"

#include <utility>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace functions_on_types = test::api::functions_on_types;

TEST(JsonSchemaCompilerFunctionsOnTypesTest, StorageAreaGetParamsCreate) {
  {
    auto params_value = std::make_unique<base::ListValue>();
    std::unique_ptr<functions_on_types::StorageArea::Get::Params> params(
        functions_on_types::StorageArea::Get::Params::Create(*params_value));
    ASSERT_TRUE(params);
    EXPECT_FALSE(params->keys);
  }
  {
    auto params_value = std::make_unique<base::ListValue>();
    params_value->AppendInteger(9);
    std::unique_ptr<functions_on_types::StorageArea::Get::Params> params(
        functions_on_types::StorageArea::Get::Params::Create(*params_value));
    EXPECT_FALSE(params);
  }
  {
    auto params_value = std::make_unique<base::ListValue>();
    params_value->AppendString("test");
    std::unique_ptr<functions_on_types::StorageArea::Get::Params> params(
        functions_on_types::StorageArea::Get::Params::Create(*params_value));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->keys);
    EXPECT_EQ("test", *params->keys->as_string);
  }
  {
    auto keys_object_value = std::make_unique<base::DictionaryValue>();
    keys_object_value->SetInteger("integer", 5);
    keys_object_value->SetString("string", "string");
    auto params_value = std::make_unique<base::ListValue>();
    params_value->Append(keys_object_value->CreateDeepCopy());
    std::unique_ptr<functions_on_types::StorageArea::Get::Params> params(
        functions_on_types::StorageArea::Get::Params::Create(*params_value));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->keys);
    EXPECT_TRUE(keys_object_value->Equals(
        &params->keys->as_object->additional_properties));
  }
}

TEST(JsonSchemaCompilerFunctionsOnTypesTest, StorageAreaGetResultCreate) {
  functions_on_types::StorageArea::Get::Results::Items items;
  items.additional_properties.SetDouble("asdf", 0.1);
  items.additional_properties.SetString("sdfg", "zxcv");
  std::unique_ptr<base::ListValue> results =
      functions_on_types::StorageArea::Get::Results::Create(items);
  base::DictionaryValue* item_result = NULL;
  ASSERT_TRUE(results->GetDictionary(0, &item_result));
  EXPECT_TRUE(item_result->Equals(&items.additional_properties));
}

TEST(JsonSchemaCompilerFunctionsOnTypesTest, ChromeSettingGetParamsCreate) {
  auto details_value = std::make_unique<base::DictionaryValue>();
  details_value->SetBoolean("incognito", true);
  auto params_value = std::make_unique<base::ListValue>();
  params_value->Append(std::move(details_value));
  std::unique_ptr<functions_on_types::ChromeSetting::Get::Params> params(
      functions_on_types::ChromeSetting::Get::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  EXPECT_TRUE(*params->details.incognito);
}
