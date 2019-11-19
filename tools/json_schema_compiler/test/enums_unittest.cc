// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/enums.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/test_util.h"

namespace enums = test::api::enums;
using json_schema_compiler::test_util::List;

TEST(JsonSchemaCompilerEnumsTest, EnumTypePopulate) {
  {
    enums::EnumType enum_type;
    base::DictionaryValue value;
    value.SetString("type", "one");
    EXPECT_TRUE(enums::EnumType::Populate(value, &enum_type));
    EXPECT_EQ(enums::ENUMERATION_ONE, enum_type.type);
    EXPECT_TRUE(value.Equals(enum_type.ToValue().get()));
  }
  {
    enums::EnumType enum_type;
    base::DictionaryValue value;
    value.SetString("type", "invalid");
    EXPECT_FALSE(enums::EnumType::Populate(value, &enum_type));
  }
}

TEST(JsonSchemaCompilerEnumsTest, EnumsAsTypes) {
  {
    base::ListValue args;
    args.AppendString("one");

    std::unique_ptr<enums::TakesEnumAsType::Params> params(
        enums::TakesEnumAsType::Params::Create(args));
    ASSERT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_ONE, params->enumeration);

    EXPECT_TRUE(args.Equals(
        enums::ReturnsEnumAsType::Results::Create(enums::ENUMERATION_ONE)
            .get()));
  }
  {
    enums::HasEnumeration enumeration;
    EXPECT_EQ(enums::ENUMERATION_NONE, enumeration.enumeration);
    EXPECT_EQ(enums::ENUMERATION_NONE, enumeration.optional_enumeration);
  }
  {
    enums::HasEnumeration enumeration;
    base::DictionaryValue value;
    ASSERT_FALSE(enums::HasEnumeration::Populate(value, &enumeration));

    value.SetString("enumeration", "one");
    ASSERT_TRUE(enums::HasEnumeration::Populate(value, &enumeration));
    EXPECT_TRUE(value.Equals(enumeration.ToValue().get()));

    value.SetString("optional_enumeration", "two");
    ASSERT_TRUE(enums::HasEnumeration::Populate(value, &enumeration));
    EXPECT_TRUE(value.Equals(enumeration.ToValue().get()));
  }
  {
    enums::ReferenceEnum enumeration;
    base::DictionaryValue value;
    ASSERT_FALSE(enums::ReferenceEnum::Populate(value, &enumeration));

    value.SetString("reference_enum", "one");
    ASSERT_TRUE(enums::ReferenceEnum::Populate(value, &enumeration));
    EXPECT_TRUE(value.Equals(enumeration.ToValue().get()));
  }
}

TEST(JsonSchemaCompilerEnumsTest, EnumsArrayAsType) {
  {
    base::ListValue params_value;
    params_value.Append(List(std::make_unique<base::Value>("one"),
                             std::make_unique<base::Value>("two")));
    std::unique_ptr<enums::TakesEnumArrayAsType::Params> params(
        enums::TakesEnumArrayAsType::Params::Create(params_value));
    ASSERT_TRUE(params);
    EXPECT_EQ(2U, params->values.size());
    EXPECT_EQ(enums::ENUMERATION_ONE, params->values[0]);
    EXPECT_EQ(enums::ENUMERATION_TWO, params->values[1]);
  }
  {
    base::ListValue params_value;
    params_value.Append(List(std::make_unique<base::Value>("invalid")));
    std::unique_ptr<enums::TakesEnumArrayAsType::Params> params(
        enums::TakesEnumArrayAsType::Params::Create(params_value));
    EXPECT_FALSE(params);
  }
}

TEST(JsonSchemaCompilerEnumsTest, ReturnsEnumCreate) {
  {
    enums::Enumeration state = enums::ENUMERATION_ONE;
    auto result = std::make_unique<base::Value>(ToString(state));
    auto expected = std::make_unique<base::Value>("one");
    EXPECT_TRUE(result->Equals(expected.get()));
  }
  {
    enums::Enumeration state = enums::ENUMERATION_ONE;
    std::unique_ptr<base::ListValue> results =
        enums::ReturnsEnum::Results::Create(state);
    base::ListValue expected;
    expected.AppendString("one");
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerEnumsTest, ReturnsTwoEnumsCreate) {
  {
    std::unique_ptr<base::ListValue> results =
        enums::ReturnsTwoEnums::Results::Create(enums::ENUMERATION_ONE,
                                                enums::OTHER_ENUMERATION_HAM);
    base::ListValue expected;
    expected.AppendString("one");
    expected.AppendString("ham");
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerEnumsTest, OptionalEnumTypePopulate) {
  {
    enums::OptionalEnumType enum_type;
    base::DictionaryValue value;
    value.SetString("type", "two");
    EXPECT_TRUE(enums::OptionalEnumType::Populate(value, &enum_type));
    EXPECT_EQ(enums::ENUMERATION_TWO, enum_type.type);
    EXPECT_TRUE(value.Equals(enum_type.ToValue().get()));
  }
  {
    enums::OptionalEnumType enum_type;
    base::DictionaryValue value;
    EXPECT_TRUE(enums::OptionalEnumType::Populate(value, &enum_type));
    EXPECT_EQ(enums::ENUMERATION_NONE, enum_type.type);
    EXPECT_TRUE(value.Equals(enum_type.ToValue().get()));
  }
  {
    enums::OptionalEnumType enum_type;
    base::DictionaryValue value;
    value.SetString("type", "invalid");
    EXPECT_FALSE(enums::OptionalEnumType::Populate(value, &enum_type));
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesEnumParamsCreate) {
  {
    base::ListValue params_value;
    params_value.AppendString("two");
    std::unique_ptr<enums::TakesEnum::Params> params(
        enums::TakesEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_TWO, params->state);
  }
  {
    base::ListValue params_value;
    params_value.AppendString("invalid");
    std::unique_ptr<enums::TakesEnum::Params> params(
        enums::TakesEnum::Params::Create(params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesEnumArrayParamsCreate) {
  {
    base::ListValue params_value;
    params_value.Append(List(std::make_unique<base::Value>("one"),
                             std::make_unique<base::Value>("two")));
    std::unique_ptr<enums::TakesEnumArray::Params> params(
        enums::TakesEnumArray::Params::Create(params_value));
    ASSERT_TRUE(params);
    EXPECT_EQ(2U, params->values.size());
    EXPECT_EQ(enums::ENUMERATION_ONE, params->values[0]);
    EXPECT_EQ(enums::ENUMERATION_TWO, params->values[1]);
  }
  {
    base::ListValue params_value;
    params_value.Append(List(std::make_unique<base::Value>("invalid")));
    std::unique_ptr<enums::TakesEnumArray::Params> params(
        enums::TakesEnumArray::Params::Create(params_value));
    EXPECT_FALSE(params);
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesOptionalEnumParamsCreate) {
  {
    base::ListValue params_value;
    params_value.AppendString("three");
    std::unique_ptr<enums::TakesOptionalEnum::Params> params(
        enums::TakesOptionalEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_THREE, params->state);
  }
  {
    base::ListValue params_value;
    std::unique_ptr<enums::TakesOptionalEnum::Params> params(
        enums::TakesOptionalEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_NONE, params->state);
  }
  {
    base::ListValue params_value;
    params_value.AppendString("invalid");
    std::unique_ptr<enums::TakesOptionalEnum::Params> params(
        enums::TakesOptionalEnum::Params::Create(params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesMultipleOptionalEnumsParamsCreate) {
  {
    base::ListValue params_value;
    params_value.AppendString("one");
    params_value.AppendString("ham");
    std::unique_ptr<enums::TakesMultipleOptionalEnums::Params> params(
        enums::TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_ONE, params->state);
    EXPECT_EQ(enums::OTHER_ENUMERATION_HAM, params->type);
  }
  {
    base::ListValue params_value;
    params_value.AppendString("one");
    std::unique_ptr<enums::TakesMultipleOptionalEnums::Params> params(
        enums::TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_ONE, params->state);
    EXPECT_EQ(enums::OTHER_ENUMERATION_NONE, params->type);
  }
  {
    base::ListValue params_value;
    std::unique_ptr<enums::TakesMultipleOptionalEnums::Params> params(
        enums::TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_NONE, params->state);
    EXPECT_EQ(enums::OTHER_ENUMERATION_NONE, params->type);
  }
  {
    base::ListValue params_value;
    params_value.AppendString("three");
    params_value.AppendString("invalid");
    std::unique_ptr<enums::TakesMultipleOptionalEnums::Params> params(
        enums::TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, OnEnumFiredCreate) {
  {
    enums::Enumeration some_enum = enums::ENUMERATION_ONE;
    auto result = std::make_unique<base::Value>(ToString(some_enum));
    auto expected = std::make_unique<base::Value>("one");
    EXPECT_TRUE(result->Equals(expected.get()));
  }
  {
    enums::Enumeration some_enum = enums::ENUMERATION_ONE;
    std::unique_ptr<base::ListValue> results(
        enums::OnEnumFired::Create(some_enum));
    base::ListValue expected;
    expected.AppendString("one");
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerEnumsTest, OnTwoEnumsFiredCreate) {
  {
    std::unique_ptr<base::Value> results(enums::OnTwoEnumsFired::Create(
        enums::ENUMERATION_ONE, enums::OTHER_ENUMERATION_HAM));
    base::ListValue expected;
    expected.AppendString("one");
    expected.AppendString("ham");
    EXPECT_TRUE(results->Equals(&expected));
  }
}
