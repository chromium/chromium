// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/enums.h"

#include <vector>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/test_util.h"

namespace enums = test::api::enums;
using json_schema_compiler::test_util::Dictionary;
using json_schema_compiler::test_util::List;

TEST(JsonSchemaCompilerEnumsTest, EnumTypePopulate) {
  {
    enums::EnumType enum_type;
    base::Value value = Dictionary("type", base::Value("one"));
    EXPECT_TRUE(enums::EnumType::Populate(value, &enum_type));
    EXPECT_EQ(enums::ENUMERATION_ONE, enum_type.type);
    EXPECT_EQ(value, enum_type.ToValue());
  }
  {
    enums::EnumType enum_type;
    base::Value value = Dictionary("type", base::Value("invalid"));
    EXPECT_FALSE(enums::EnumType::Populate(value, &enum_type));
  }
}

TEST(JsonSchemaCompilerEnumsTest, EnumsAsTypes) {
  {
    base::Value::List args;
    args.Append("one");

    std::unique_ptr<enums::TakesEnumAsType::Params> params(
        enums::TakesEnumAsType::Params::Create(args));
    ASSERT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_ONE, params->enumeration);

    EXPECT_EQ(args, enums::ReturnsEnumAsType::Results::Create(
                        enums::ENUMERATION_ONE));
  }
  {
    enums::HasEnumeration enumeration;
    EXPECT_EQ(enums::ENUMERATION_NONE, enumeration.enumeration);
    EXPECT_EQ(enums::ENUMERATION_NONE, enumeration.optional_enumeration);
  }
  {
    enums::HasEnumeration enumeration;
    base::Value value(base::Value::Type::DICT);
    ASSERT_FALSE(enums::HasEnumeration::Populate(value, &enumeration));

    value.GetDict().Set("enumeration", "one");
    ASSERT_TRUE(enums::HasEnumeration::Populate(value, &enumeration));
    EXPECT_EQ(value, enumeration.ToValue());

    value.GetDict().Set("optional_enumeration", "two");
    ASSERT_TRUE(enums::HasEnumeration::Populate(value, &enumeration));
    EXPECT_EQ(value, enumeration.ToValue());
  }
  {
    enums::ReferenceEnum enumeration;
    base::Value value(base::Value::Type::DICT);
    ASSERT_FALSE(enums::ReferenceEnum::Populate(value, &enumeration));

    value.GetDict().Set("reference_enum", "one");
    ASSERT_TRUE(enums::ReferenceEnum::Populate(value, &enumeration));
    EXPECT_EQ(value, enumeration.ToValue());
  }
}

TEST(JsonSchemaCompilerEnumsTest, EnumsArrayAsType) {
  {
    base::Value::List params_value;
    params_value.Append(List(base::Value("one"), base::Value("two")));
    std::unique_ptr<enums::TakesEnumArrayAsType::Params> params(
        enums::TakesEnumArrayAsType::Params::Create(params_value));
    ASSERT_TRUE(params);
    EXPECT_EQ(2U, params->values.size());
    EXPECT_EQ(enums::ENUMERATION_ONE, params->values[0]);
    EXPECT_EQ(enums::ENUMERATION_TWO, params->values[1]);
  }
  {
    base::Value::List params_value;
    params_value.Append(List(base::Value("invalid")));
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
    EXPECT_EQ(*result, *expected);
  }
  {
    enums::Enumeration state = enums::ENUMERATION_ONE;
    base::Value results(enums::ReturnsEnum::Results::Create(state));
    base::Value::List expected;
    expected.Append("one");
    EXPECT_EQ(expected, results);
  }
}

TEST(JsonSchemaCompilerEnumsTest, ReturnsTwoEnumsCreate) {
  {
    base::Value results(enums::ReturnsTwoEnums::Results::Create(
        enums::ENUMERATION_ONE, enums::OTHER_ENUMERATION_HAM));
    base::Value::List expected;
    expected.Append("one");
    expected.Append("ham");
    EXPECT_EQ(expected, results);
  }
}

TEST(JsonSchemaCompilerEnumsTest, OptionalEnumTypePopulate) {
  {
    enums::OptionalEnumType enum_type;
    base::Value value = Dictionary("type", base::Value("two"));
    EXPECT_TRUE(enums::OptionalEnumType::Populate(value, &enum_type));
    EXPECT_EQ(enums::ENUMERATION_TWO, enum_type.type);
    EXPECT_EQ(value, enum_type.ToValue());
  }
  {
    enums::OptionalEnumType enum_type;
    base::Value value(base::Value::Type::DICT);
    EXPECT_TRUE(enums::OptionalEnumType::Populate(value, &enum_type));
    EXPECT_EQ(enums::ENUMERATION_NONE, enum_type.type);
    EXPECT_EQ(value, enum_type.ToValue());
  }
  {
    enums::OptionalEnumType enum_type;
    base::Value value = Dictionary("type", base::Value("invalid"));
    EXPECT_FALSE(enums::OptionalEnumType::Populate(value, &enum_type));
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesEnumParamsCreate) {
  {
    base::Value::List params_value;
    params_value.Append("two");
    std::unique_ptr<enums::TakesEnum::Params> params(
        enums::TakesEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_TWO, params->state);
  }
  {
    base::Value::List params_value;
    params_value.Append("invalid");
    std::unique_ptr<enums::TakesEnum::Params> params(
        enums::TakesEnum::Params::Create(params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesEnumArrayParamsCreate) {
  {
    base::Value::List params_value;
    params_value.Append(List(base::Value("one"), base::Value("two")));
    std::unique_ptr<enums::TakesEnumArray::Params> params(
        enums::TakesEnumArray::Params::Create(params_value));
    ASSERT_TRUE(params);
    EXPECT_EQ(2U, params->values.size());
    EXPECT_EQ(enums::ENUMERATION_ONE, params->values[0]);
    EXPECT_EQ(enums::ENUMERATION_TWO, params->values[1]);
  }
  {
    base::Value::List params_value;
    params_value.Append(List(base::Value("invalid")));
    std::unique_ptr<enums::TakesEnumArray::Params> params(
        enums::TakesEnumArray::Params::Create(params_value));
    EXPECT_FALSE(params);
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesOptionalEnumParamsCreate) {
  {
    base::Value::List params_value;
    params_value.Append("three");
    std::unique_ptr<enums::TakesOptionalEnum::Params> params(
        enums::TakesOptionalEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_THREE, params->state);
  }
  {
    base::Value::List params_value;
    std::unique_ptr<enums::TakesOptionalEnum::Params> params(
        enums::TakesOptionalEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_NONE, params->state);
  }
  {
    base::Value::List params_value;
    params_value.Append("invalid");
    std::unique_ptr<enums::TakesOptionalEnum::Params> params(
        enums::TakesOptionalEnum::Params::Create(params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesMultipleOptionalEnumsParamsCreate) {
  {
    base::Value::List params_value;
    params_value.Append("one");
    params_value.Append("ham");
    std::unique_ptr<enums::TakesMultipleOptionalEnums::Params> params(
        enums::TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_ONE, params->state);
    EXPECT_EQ(enums::OTHER_ENUMERATION_HAM, params->type);
  }
  {
    base::Value::List params_value;
    params_value.Append("one");
    std::unique_ptr<enums::TakesMultipleOptionalEnums::Params> params(
        enums::TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_ONE, params->state);
    EXPECT_EQ(enums::OTHER_ENUMERATION_NONE, params->type);
  }
  {
    base::Value::List params_value;
    std::unique_ptr<enums::TakesMultipleOptionalEnums::Params> params(
        enums::TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(enums::ENUMERATION_NONE, params->state);
    EXPECT_EQ(enums::OTHER_ENUMERATION_NONE, params->type);
  }
  {
    base::Value::List params_value;
    params_value.Append("three");
    params_value.Append("invalid");
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
    EXPECT_EQ(*result, *expected);
  }
  {
    enums::Enumeration some_enum = enums::ENUMERATION_ONE;
    base::Value results(enums::OnEnumFired::Create(some_enum));
    base::Value::List expected;
    expected.Append("one");
    EXPECT_EQ(expected, results);
  }
}

TEST(JsonSchemaCompilerEnumsTest, OnTwoEnumsFiredCreate) {
  {
    base::Value results(enums::OnTwoEnumsFired::Create(
        enums::ENUMERATION_ONE, enums::OTHER_ENUMERATION_HAM));
    base::Value::List expected;
    expected.Append("one");
    expected.Append("ham");
    EXPECT_EQ(expected, results);
  }
}
