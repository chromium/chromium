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
    EXPECT_TRUE(enums::EnumType::Populate(value.GetDict(), enum_type));
    EXPECT_EQ(enums::Enumeration::kOne, enum_type.type);
    EXPECT_EQ(value, enum_type.ToValue());
  }
  {
    enums::EnumType enum_type;
    base::Value value = Dictionary("type", base::Value("invalid"));
    EXPECT_FALSE(enums::EnumType::Populate(value.GetDict(), enum_type));
  }
}

TEST(JsonSchemaCompilerEnumsTest, EnumsAsTypes) {
  {
    base::Value::List args;
    args.Append("one");

    absl::optional<enums::TakesEnumAsType::Params> params(
        enums::TakesEnumAsType::Params::Create(args));
    ASSERT_TRUE(params.has_value());
    EXPECT_EQ(enums::Enumeration::kOne, params->enumeration);

    EXPECT_EQ(args, enums::ReturnsEnumAsType::Results::Create(
                        enums::Enumeration::kOne));
  }
  {
    enums::HasEnumeration enumeration;
    EXPECT_EQ(enums::Enumeration::kNone, enumeration.enumeration);
    EXPECT_EQ(enums::Enumeration::kNone, enumeration.optional_enumeration);
  }
  {
    enums::HasEnumeration enumeration;
    base::Value::Dict value;
    ASSERT_FALSE(enums::HasEnumeration::Populate(value, enumeration));

    value.Set("enumeration", "one");
    ASSERT_TRUE(enums::HasEnumeration::Populate(value, enumeration));
    EXPECT_EQ(value, enumeration.ToValue());

    value.Set("optional_enumeration", "two");
    ASSERT_TRUE(enums::HasEnumeration::Populate(value, enumeration));
    EXPECT_EQ(value, enumeration.ToValue());
  }
  {
    enums::ReferenceEnum enumeration;
    base::Value::Dict value;
    ASSERT_FALSE(enums::ReferenceEnum::Populate(value, enumeration));

    value.Set("reference_enum", "one");
    ASSERT_TRUE(enums::ReferenceEnum::Populate(value, enumeration));
    EXPECT_EQ(value, enumeration.ToValue());
  }
}

TEST(JsonSchemaCompilerEnumsTest, EnumsArrayAsType) {
  {
    base::Value::List params_value;
    params_value.Append(List(base::Value("one"), base::Value("two")));
    absl::optional<enums::TakesEnumArrayAsType::Params> params(
        enums::TakesEnumArrayAsType::Params::Create(params_value));
    ASSERT_TRUE(params);
    EXPECT_EQ(2U, params->values.size());
    EXPECT_EQ(enums::Enumeration::kOne, params->values[0]);
    EXPECT_EQ(enums::Enumeration::kTwo, params->values[1]);
  }
  {
    base::Value::List params_value;
    params_value.Append(List(base::Value("invalid")));
    absl::optional<enums::TakesEnumArrayAsType::Params> params(
        enums::TakesEnumArrayAsType::Params::Create(params_value));
    EXPECT_FALSE(params);
  }
}

TEST(JsonSchemaCompilerEnumsTest, ReturnsEnumCreate) {
  {
    enums::Enumeration state = enums::Enumeration::kOne;
    auto result = std::make_unique<base::Value>(ToString(state));
    auto expected = std::make_unique<base::Value>("one");
    EXPECT_EQ(*result, *expected);
  }
  {
    enums::Enumeration state = enums::Enumeration::kOne;
    base::Value results(enums::ReturnsEnum::Results::Create(state));
    base::Value::List expected;
    expected.Append("one");
    EXPECT_EQ(expected, results);
  }
}

TEST(JsonSchemaCompilerEnumsTest, ReturnsTwoEnumsCreate) {
  {
    base::Value results(enums::ReturnsTwoEnums::Results::Create(
        enums::Enumeration::kOne, enums::OtherEnumeration::kHam));
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
    EXPECT_TRUE(enums::OptionalEnumType::Populate(value.GetDict(), enum_type));
    EXPECT_EQ(enums::Enumeration::kTwo, enum_type.type);
    EXPECT_EQ(value, enum_type.ToValue());
  }
  {
    enums::OptionalEnumType enum_type;
    base::Value value(base::Value::Type::DICT);
    EXPECT_TRUE(enums::OptionalEnumType::Populate(value.GetDict(), enum_type));
    EXPECT_EQ(enums::Enumeration::kNone, enum_type.type);
    EXPECT_EQ(value, enum_type.ToValue());
  }
  {
    enums::OptionalEnumType enum_type;
    base::Value value = Dictionary("type", base::Value("invalid"));
    EXPECT_FALSE(enums::OptionalEnumType::Populate(value.GetDict(), enum_type));
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesEnumParamsCreate) {
  {
    base::Value::List params_value;
    params_value.Append("two");
    absl::optional<enums::TakesEnum::Params> params(
        enums::TakesEnum::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_EQ(enums::Enumeration::kTwo, params->state);
  }
  {
    base::Value::List params_value;
    params_value.Append("invalid");
    absl::optional<enums::TakesEnum::Params> params(
        enums::TakesEnum::Params::Create(params_value));
    EXPECT_FALSE(params.has_value());
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesEnumArrayParamsCreate) {
  {
    base::Value::List params_value;
    params_value.Append(List(base::Value("one"), base::Value("two")));
    absl::optional<enums::TakesEnumArray::Params> params(
        enums::TakesEnumArray::Params::Create(params_value));
    ASSERT_TRUE(params);
    EXPECT_EQ(2U, params->values.size());
    EXPECT_EQ(enums::Enumeration::kOne, params->values[0]);
    EXPECT_EQ(enums::Enumeration::kTwo, params->values[1]);
  }
  {
    base::Value::List params_value;
    params_value.Append(List(base::Value("invalid")));
    absl::optional<enums::TakesEnumArray::Params> params(
        enums::TakesEnumArray::Params::Create(params_value));
    EXPECT_FALSE(params);
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesOptionalEnumParamsCreate) {
  {
    base::Value::List params_value;
    params_value.Append("three");
    absl::optional<enums::TakesOptionalEnum::Params> params(
        enums::TakesOptionalEnum::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_EQ(enums::Enumeration::kThree, params->state);
  }
  {
    base::Value::List params_value;
    absl::optional<enums::TakesOptionalEnum::Params> params(
        enums::TakesOptionalEnum::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_EQ(enums::Enumeration::kNone, params->state);
  }
  {
    base::Value::List params_value;
    params_value.Append("invalid");
    absl::optional<enums::TakesOptionalEnum::Params> params(
        enums::TakesOptionalEnum::Params::Create(params_value));
    EXPECT_FALSE(params.has_value());
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesMultipleOptionalEnumsParamsCreate) {
  {
    base::Value::List params_value;
    params_value.Append("one");
    params_value.Append("ham");
    absl::optional<enums::TakesMultipleOptionalEnums::Params> params(
        enums::TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_EQ(enums::Enumeration::kOne, params->state);
    EXPECT_EQ(enums::OtherEnumeration::kHam, params->type);
  }
  {
    base::Value::List params_value;
    params_value.Append("one");
    absl::optional<enums::TakesMultipleOptionalEnums::Params> params(
        enums::TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_EQ(enums::Enumeration::kOne, params->state);
    EXPECT_EQ(enums::OtherEnumeration::kNone, params->type);
  }
  {
    base::Value::List params_value;
    absl::optional<enums::TakesMultipleOptionalEnums::Params> params(
        enums::TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_EQ(enums::Enumeration::kNone, params->state);
    EXPECT_EQ(enums::OtherEnumeration::kNone, params->type);
  }
  {
    base::Value::List params_value;
    params_value.Append("three");
    params_value.Append("invalid");
    absl::optional<enums::TakesMultipleOptionalEnums::Params> params(
        enums::TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_FALSE(params.has_value());
  }
}

TEST(JsonSchemaCompilerEnumsTest, OnEnumFiredCreate) {
  {
    enums::Enumeration some_enum = enums::Enumeration::kOne;
    auto result = std::make_unique<base::Value>(ToString(some_enum));
    auto expected = std::make_unique<base::Value>("one");
    EXPECT_EQ(*result, *expected);
  }
  {
    enums::Enumeration some_enum = enums::Enumeration::kOne;
    base::Value results(enums::OnEnumFired::Create(some_enum));
    base::Value::List expected;
    expected.Append("one");
    EXPECT_EQ(expected, results);
  }
}

TEST(JsonSchemaCompilerEnumsTest, OnTwoEnumsFiredCreate) {
  {
    base::Value results(enums::OnTwoEnumsFired::Create(
        enums::Enumeration::kOne, enums::OtherEnumeration::kHam));
    base::Value::List expected;
    expected.Append("one");
    expected.Append("ham");
    EXPECT_EQ(expected, results);
  }
}

TEST(JsonSchemaCompilerEnumsTest, EnumCaptalisationTest) {
  // This test validates that we keep consistent kCaptalisation for different
  // formats for the value the enum is mapping.
  EXPECT_EQ(enums::EnumNameTransformation::kFirstEntryExample,
            enums::ParseEnumNameTransformation("FIRST_ENTRY_EXAMPLE"));
  EXPECT_EQ(enums::EnumNameTransformation::kSecondEntryExample,
            enums::ParseEnumNameTransformation("second_entry_example"));
  EXPECT_EQ(enums::EnumNameTransformation::kThirdEntryExample,
            enums::ParseEnumNameTransformation("thirdEntryExample"));
  EXPECT_EQ(enums::EnumNameTransformation::kFourthEntryExample,
            enums::ParseEnumNameTransformation("FourthEntryExample"));
  EXPECT_EQ(enums::EnumNameTransformation::kFifthEntryExample1234,
            enums::ParseEnumNameTransformation("FIFTH_ENTRY_EXAMPLE_1234"));
}
