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

static base::Value CreateBasicArrayTypeDictionary() {
  base::Value value(base::Value::Type::DICTIONARY);
  base::Value strings_value(base::Value::Type::LIST);
  strings_value.Append("a");
  strings_value.Append("b");
  strings_value.Append("c");
  strings_value.Append("it's easy as");
  base::Value integers_value(base::Value::Type::LIST);
  integers_value.Append(1);
  integers_value.Append(2);
  integers_value.Append(3);
  base::Value booleans_value(base::Value::Type::LIST);
  booleans_value.Append(false);
  booleans_value.Append(true);
  base::Value numbers_value(base::Value::Type::LIST);
  numbers_value.Append(6.1);
  value.SetPath("numbers", std::move(numbers_value));
  value.SetPath("booleans", std::move(booleans_value));
  value.SetPath("strings", std::move(strings_value));
  value.SetPath("integers", std::move(integers_value));
  return value;
}

base::Value CreateItemValue(int val) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetIntPath("val", val);
  return value;
}

}  // namespace

TEST(JsonSchemaCompilerArrayTest, BasicArrayType) {
  {
    base::Value value = CreateBasicArrayTypeDictionary();
    auto basic_array_type = std::make_unique<arrays::BasicArrayType>();
    ASSERT_TRUE(
        arrays::BasicArrayType::Populate(value, basic_array_type.get()));
    EXPECT_EQ(value, basic_array_type->ToValue());
  }
}

TEST(JsonSchemaCompilerArrayTest, EnumArrayReference) {
  // { "types": ["one", "two", "three"] }
  base::Value types(base::Value::Type::LIST);
  types.Append("one");
  types.Append("two");
  types.Append("three");
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetPath("types", std::move(types));

  arrays::EnumArrayReference enum_array_reference;

  // Test Populate.
  ASSERT_TRUE(
      arrays::EnumArrayReference::Populate(value, &enum_array_reference));

  arrays::Enumeration expected_types[] = {arrays::ENUMERATION_ONE,
                                          arrays::ENUMERATION_TWO,
                                          arrays::ENUMERATION_THREE};
  EXPECT_EQ(std::vector<arrays::Enumeration>(
                expected_types, expected_types + std::size(expected_types)),
            enum_array_reference.types);

  // Test ToValue.
  base::Value::Dict as_value(enum_array_reference.ToValue());
  EXPECT_EQ(value, as_value);
}

TEST(JsonSchemaCompilerArrayTest, EnumArrayMixed) {
  // { "types": ["one", "two", "three"] }
  base::Value infile_enums(base::Value::Type::LIST);
  infile_enums.Append("one");
  infile_enums.Append("two");
  infile_enums.Append("three");

  base::Value external_enums(base::Value::Type::LIST);
  external_enums.Append("one");
  external_enums.Append("two");
  external_enums.Append("three");

  base::Value value(base::Value::Type::DICTIONARY);
  value.SetPath("infile_enums", std::move(infile_enums));
  value.SetPath("external_enums", std::move(external_enums));

  arrays::EnumArrayMixed enum_array_mixed;

  // Test Populate.
  ASSERT_TRUE(arrays::EnumArrayMixed::Populate(value, &enum_array_mixed));

  arrays::Enumeration expected_infile_types[] = {arrays::ENUMERATION_ONE,
                                                 arrays::ENUMERATION_TWO,
                                                 arrays::ENUMERATION_THREE};
  EXPECT_EQ(std::vector<arrays::Enumeration>(
                expected_infile_types,
                expected_infile_types + std::size(expected_infile_types)),
            enum_array_mixed.infile_enums);

  test::api::enums::Enumeration expected_external_types[] = {
      test::api::enums::ENUMERATION_ONE, test::api::enums::ENUMERATION_TWO,
      test::api::enums::ENUMERATION_THREE};
  EXPECT_EQ(std::vector<test::api::enums::Enumeration>(
                expected_external_types,
                expected_external_types + std::size(expected_external_types)),
            enum_array_mixed.external_enums);

  // Test ToValue.
  base::Value::Dict as_value(enum_array_mixed.ToValue());
  EXPECT_EQ(value, as_value);
}

TEST(JsonSchemaCompilerArrayTest, OptionalEnumArrayType) {
  {
    std::vector<arrays::Enumeration> enums;
    enums.push_back(arrays::ENUMERATION_ONE);
    enums.push_back(arrays::ENUMERATION_TWO);
    enums.push_back(arrays::ENUMERATION_THREE);

    base::Value types(base::Value::Type::LIST);
    for (auto& enum_entry : enums)
      types.Append(ToString(enum_entry));

    base::Value value(base::Value::Type::DICTIONARY);
    value.SetPath("types", std::move(types));

    arrays::OptionalEnumArrayType enum_array_type;
    ASSERT_TRUE(
        arrays::OptionalEnumArrayType::Populate(value, &enum_array_type));
    EXPECT_EQ(enums, *enum_array_type.types);
  }
  {
    base::Value value(base::Value::Type::DICTIONARY);
    base::Value enum_array(base::Value::Type::LIST);
    enum_array.Append("invalid");

    value.SetPath("types", std::move(enum_array));
    arrays::OptionalEnumArrayType enum_array_type;
    ASSERT_FALSE(
        arrays::OptionalEnumArrayType::Populate(value, &enum_array_type));
    EXPECT_TRUE(enum_array_type.types->empty());
  }
}

TEST(JsonSchemaCompilerArrayTest, RefArrayType) {
  {
    base::Value value(base::Value::Type::DICTIONARY);
    base::Value ref_array(base::Value::Type::LIST);
    ref_array.Append(CreateItemValue(1));
    ref_array.Append(CreateItemValue(2));
    ref_array.Append(CreateItemValue(3));
    value.SetPath("refs", std::move(ref_array));
    auto ref_array_type = std::make_unique<arrays::RefArrayType>();
    EXPECT_TRUE(arrays::RefArrayType::Populate(value, ref_array_type.get()));
    ASSERT_EQ(3u, ref_array_type->refs.size());
    EXPECT_EQ(1, ref_array_type->refs[0].val);
    EXPECT_EQ(2, ref_array_type->refs[1].val);
    EXPECT_EQ(3, ref_array_type->refs[2].val);
  }
  {
    base::Value value(base::Value::Type::DICTIONARY);
    base::Value not_ref_array(base::Value::Type::LIST);
    not_ref_array.Append(CreateItemValue(1));
    not_ref_array.Append(3);
    value.SetPath("refs", std::move(not_ref_array));
    auto ref_array_type = std::make_unique<arrays::RefArrayType>();
    EXPECT_FALSE(arrays::RefArrayType::Populate(value, ref_array_type.get()));
  }
}

TEST(JsonSchemaCompilerArrayTest, IntegerArrayParamsCreate) {
  base::Value::List params_value;
  base::Value::List integer_array;
  integer_array.Append(2);
  integer_array.Append(4);
  integer_array.Append(8);
  params_value.Append(std::move(integer_array));
  std::unique_ptr<arrays::IntegerArray::Params> params(
      arrays::IntegerArray::Params::Create(params_value));
  EXPECT_TRUE(params.get());
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
  std::unique_ptr<arrays::AnyArray::Params> params(
      arrays::AnyArray::Params::Create(params_value));
  EXPECT_TRUE(params.get());
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
  std::unique_ptr<arrays::ObjectArray::Params> params(
      arrays::ObjectArray::Params::Create(params_value));
  EXPECT_TRUE(params.get());
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
  std::unique_ptr<arrays::RefArray::Params> params(
      arrays::RefArray::Params::Create(params_value));
  EXPECT_TRUE(params.get());
  ASSERT_EQ(2u, params->refs.size());
  EXPECT_EQ(1, params->refs[0].val);
  EXPECT_EQ(2, params->refs[1].val);
}

TEST(JsonSchemaCompilerArrayTest, ReturnIntegerArrayResultCreate) {
  std::vector<int> integers;
  integers.push_back(1);
  integers.push_back(2);
  base::Value results(arrays::ReturnIntegerArray::Results::Create(integers));

  base::Value expected(base::Value::Type::LIST);
  base::Value expected_argument(base::Value::Type::LIST);
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

  base::Value expected(base::Value::Type::LIST);
  base::Value expected_argument(base::Value::Type::LIST);
  base::Value first(base::Value::Type::DICTIONARY);
  first.SetIntPath("val", 1);
  expected_argument.Append(std::move(first));
  base::Value second(base::Value::Type::DICTIONARY);
  second.SetIntPath("val", 2);
  expected_argument.Append(std::move(second));
  expected.Append(std::move(expected_argument));
  EXPECT_EQ(expected, results);
}
