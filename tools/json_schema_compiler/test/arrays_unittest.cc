// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/arrays.h"

#include <stddef.h>

#include <iterator>
#include <memory>
#include <utility>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/enums.h"

namespace arrays = test::api::arrays;

namespace {

static base::Value::Dict CreateBasicArrayTypeDictionary() {
  base::Value::Dict value;
  base::Value::List strings_value;
  strings_value.Append("a");
  strings_value.Append("b");
  strings_value.Append("c");
  strings_value.Append("it's easy as");
  base::Value::List integers_value;
  integers_value.Append(1);
  integers_value.Append(2);
  integers_value.Append(3);
  base::Value::List booleans_value;
  booleans_value.Append(false);
  booleans_value.Append(true);
  base::Value::List numbers_value;
  numbers_value.Append(6.1);
  value.Set("numbers", std::move(numbers_value));
  value.Set("booleans", std::move(booleans_value));
  value.Set("strings", std::move(strings_value));
  value.Set("integers", std::move(integers_value));
  return value;
}

base::Value CreateItemValue(int val) {
  base::Value::Dict value;
  value.Set("val", val);
  return base::Value(std::move(value));
}

}  // namespace

TEST(JsonSchemaCompilerArrayTest, BasicArrayType) {
  {
    base::Value::Dict value(CreateBasicArrayTypeDictionary());
    auto basic_array_type = arrays::BasicArrayType::FromValue(value);
    ASSERT_TRUE(basic_array_type);
    EXPECT_EQ(value, basic_array_type->ToValue());

    EXPECT_EQ(basic_array_type->Clone().ToValue(), basic_array_type->ToValue());
  }
}

TEST(JsonSchemaCompilerArrayTest, EnumArrayReference) {
  // { "types": ["one", "two", "three"] }
  base::Value::List types;
  types.Append("one");
  types.Append("two");
  types.Append("three");
  base::Value::Dict value;
  value.Set("types", std::move(types));

  // Test Populate.
  auto enum_array_reference = arrays::EnumArrayReference::FromValue(value);
  ASSERT_TRUE(enum_array_reference);

  arrays::Enumeration expected_types[] = {arrays::Enumeration::kOne,
                                          arrays::Enumeration::kTwo,
                                          arrays::Enumeration::kThree};
  EXPECT_EQ(std::vector<arrays::Enumeration>(
                expected_types, expected_types + std::size(expected_types)),
            enum_array_reference->types);

  // Test ToValue.
  base::Value::Dict as_value(enum_array_reference->ToValue());
  EXPECT_EQ(value, as_value);

  EXPECT_EQ(enum_array_reference->Clone().ToValue(),
            enum_array_reference->ToValue());
}

TEST(JsonSchemaCompilerArrayTest, EnumArrayMixed) {
  // { "types": ["one", "two", "three"] }
  base::Value::List infile_enums;
  infile_enums.Append("one");
  infile_enums.Append("two");
  infile_enums.Append("three");

  base::Value::List external_enums;
  external_enums.Append("one");
  external_enums.Append("two");
  external_enums.Append("three");

  base::Value::Dict value;
  value.Set("infile_enums", std::move(infile_enums));
  value.Set("external_enums", std::move(external_enums));

  // Test Populate.
  auto enum_array_mixed = arrays::EnumArrayMixed::FromValue(value);
  ASSERT_TRUE(enum_array_mixed);

  arrays::Enumeration expected_infile_types[] = {arrays::Enumeration::kOne,
                                                 arrays::Enumeration::kTwo,
                                                 arrays::Enumeration::kThree};
  EXPECT_EQ(std::vector<arrays::Enumeration>(
                expected_infile_types,
                expected_infile_types + std::size(expected_infile_types)),
            enum_array_mixed->infile_enums);

  test::api::enums::Enumeration expected_external_types[] = {
      test::api::enums::Enumeration::kOne, test::api::enums::Enumeration::kTwo,
      test::api::enums::Enumeration::kThree};
  EXPECT_EQ(std::vector<test::api::enums::Enumeration>(
                expected_external_types,
                expected_external_types + std::size(expected_external_types)),
            enum_array_mixed->external_enums);

  // Test ToValue.
  base::Value::Dict as_value(enum_array_mixed->ToValue());
  EXPECT_EQ(value, as_value);

  EXPECT_EQ(enum_array_mixed->Clone().ToValue(), enum_array_mixed->ToValue());
}

TEST(JsonSchemaCompilerArrayTest, OptionalEnumArrayType) {
  {
    std::vector<arrays::Enumeration> enums;
    enums.push_back(arrays::Enumeration::kOne);
    enums.push_back(arrays::Enumeration::kTwo);
    enums.push_back(arrays::Enumeration::kThree);

    base::Value::List types;
    for (auto& enum_entry : enums)
      types.Append(ToString(enum_entry));

    base::Value::Dict value;
    value.Set("types", std::move(types));

    auto enum_array_type = arrays::OptionalEnumArrayType::FromValue(value);
    ASSERT_TRUE(enum_array_type);
    EXPECT_EQ(enums, *enum_array_type->types);

    EXPECT_EQ(enum_array_type->Clone().ToValue(), enum_array_type->ToValue());
  }
  {
    base::Value::Dict value;
    base::Value::List enum_array;
    enum_array.Append("invalid");

    value.Set("types", std::move(enum_array));
    auto enum_array_type = arrays::OptionalEnumArrayType::FromValue(value);
    ASSERT_FALSE(enum_array_type);
  }
}

TEST(JsonSchemaCompilerArrayTest, RefArrayType) {
  {
    base::Value::Dict value;
    base::Value::List ref_array;
    ref_array.Append(CreateItemValue(1));
    ref_array.Append(CreateItemValue(2));
    ref_array.Append(CreateItemValue(3));
    value.Set("refs", std::move(ref_array));
    auto ref_array_type = arrays::RefArrayType::FromValue(value);
    EXPECT_TRUE(ref_array_type);
    ASSERT_EQ(3u, ref_array_type->refs.size());
    EXPECT_EQ(1, ref_array_type->refs[0].val);
    EXPECT_EQ(2, ref_array_type->refs[1].val);
    EXPECT_EQ(3, ref_array_type->refs[2].val);

    EXPECT_EQ(ref_array_type->Clone().ToValue(), ref_array_type->ToValue());
  }
  {
    base::Value::Dict value;
    base::Value::List not_ref_array;
    not_ref_array.Append(CreateItemValue(1));
    not_ref_array.Append(3);
    value.Set("refs", std::move(not_ref_array));
    auto ref_array_type = arrays::RefArrayType::FromValue(value);
    EXPECT_FALSE(ref_array_type);
  }
}

TEST(JsonSchemaCompilerArrayTest, IntegerArrayParamsCreate) {
  base::Value::List params_value;
  base::Value::List integer_array;
  integer_array.Append(2);
  integer_array.Append(4);
  integer_array.Append(8);
  params_value.Append(std::move(integer_array));
  std::optional<arrays::IntegerArray::Params> params(
      arrays::IntegerArray::Params::Create(params_value));
  EXPECT_TRUE(params.has_value());
  ASSERT_EQ(3u, params->nums.size());
  EXPECT_EQ(2, params->nums[0]);
  EXPECT_EQ(4, params->nums[1]);
  EXPECT_EQ(8, params->nums[2]);
}

TEST(JsonSchemaCompilerArrayTest, AnyArrayParamsCreate) {
  base::Value::List params_value;
  base::Value::List any_array;
  any_array.Append(1);
  any_array.Append("test");
  any_array.Append(CreateItemValue(2));
  params_value.Append(std::move(any_array));
  std::optional<arrays::AnyArray::Params> params(
      arrays::AnyArray::Params::Create(params_value));
  EXPECT_TRUE(params.has_value());
  ASSERT_EQ(3u, params->anys.size());
  ASSERT_TRUE(params->anys[0].is_int());
  EXPECT_EQ(1, params->anys[0].GetInt());
}

TEST(JsonSchemaCompilerArrayTest, ObjectArrayParamsCreate) {
  base::Value::List params_value;
  base::Value::List item_array;
  item_array.Append(CreateItemValue(1));
  item_array.Append(CreateItemValue(2));
  params_value.Append(std::move(item_array));
  std::optional<arrays::ObjectArray::Params> params(
      arrays::ObjectArray::Params::Create(params_value));
  EXPECT_TRUE(params.has_value());
  ASSERT_EQ(2u, params->objects.size());
  EXPECT_EQ(1, params->objects[0].additional_properties["val"]);
  EXPECT_EQ(2, params->objects[1].additional_properties["val"]);
}

TEST(JsonSchemaCompilerArrayTest, RefArrayParamsCreate) {
  base::Value::List params_value;
  base::Value::List item_array;
  item_array.Append(CreateItemValue(1));
  item_array.Append(CreateItemValue(2));
  params_value.Append(std::move(item_array));
  std::optional<arrays::RefArray::Params> params(
      arrays::RefArray::Params::Create(params_value));
  EXPECT_TRUE(params.has_value());
  ASSERT_EQ(2u, params->refs.size());
  EXPECT_EQ(1, params->refs[0].val);
  EXPECT_EQ(2, params->refs[1].val);
}

TEST(JsonSchemaCompilerArrayTest, ReturnIntegerArrayResultCreate) {
  std::vector<int> integers;
  integers.push_back(1);
  integers.push_back(2);
  base::Value results(arrays::ReturnIntegerArray::Results::Create(integers));

  base::Value::List expected;
  base::Value::List expected_argument;
  expected_argument.Append(1);
  expected_argument.Append(2);
  expected.Append(std::move(expected_argument));
  EXPECT_EQ(expected, results);
}

TEST(JsonSchemaCompilerArrayTest, ReturnRefArrayResultCreate) {
  std::vector<arrays::Item> items;
  items.push_back(arrays::Item());
  items.push_back(arrays::Item());
  items[0].val = 1;
  items[1].val = 2;
  base::Value results(arrays::ReturnRefArray::Results::Create(items));

  base::Value::List expected;
  base::Value::List expected_argument;
  base::Value::Dict first;
  first.Set("val", 1);
  expected_argument.Append(std::move(first));
  base::Value::Dict second;
  second.Set("val", 2);
  expected_argument.Append(std::move(second));
  expected.Append(std::move(expected_argument));
  EXPECT_EQ(expected, results);
}
