// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/choices.h"

#include <stddef.h>

#include <utility>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/test_util.h"

namespace {

namespace choices = test::api::choices;
namespace TakesIntegers = choices::TakesIntegers;
using choices::NestedChoice;
using json_schema_compiler::test_util::Dictionary;
using json_schema_compiler::test_util::List;
using json_schema_compiler::test_util::ReadJson;
using json_schema_compiler::test_util::Vector;

TEST(JsonSchemaCompilerChoicesTest, TakesIntegersParamsCreate) {
  {
    std::optional<TakesIntegers::Params> params(
        TakesIntegers::Params::Create(List(base::Value(true)).GetList()));

    static_assert(!std::is_copy_constructible_v<TakesIntegers::Params>);
    static_assert(!std::is_copy_assignable_v<TakesIntegers::Params>);
    static_assert(std::is_move_constructible_v<TakesIntegers::Params>);
    static_assert(std::is_move_assignable_v<TakesIntegers::Params>);

    EXPECT_FALSE(params);
  }
  {
    std::optional<TakesIntegers::Params> params(
        TakesIntegers::Params::Create(List(base::Value(6)).GetList()));
    ASSERT_TRUE(params);
    EXPECT_FALSE(params->nums.as_integers);
    EXPECT_EQ(6, *params->nums.as_integer);
    EXPECT_EQ(6, *params->nums.Clone().as_integer);
  }
  {
    std::optional<TakesIntegers::Params> params(TakesIntegers::Params::Create(
        List(List(base::Value(2), base::Value(6), base::Value(8))).GetList()));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->nums.as_integers);
    EXPECT_EQ(Vector(2, 6, 8), *params->nums.as_integers);
    EXPECT_EQ(Vector(2, 6, 8), *params->nums.Clone().as_integers);
  }
}

TEST(JsonSchemaCompilerChoicesTest, ObjectWithChoicesParamsCreate) {
  {
    std::optional<choices::ObjectWithChoices::Params> params(
        choices::ObjectWithChoices::Params::Create(
            List(Dictionary("strings", base::Value("asdf"))).GetList()));
    ASSERT_TRUE(params);
    EXPECT_FALSE(params->string_info.strings.as_strings);
    EXPECT_EQ("asdf", *params->string_info.strings.as_string);
    EXPECT_EQ("asdf", *params->string_info.Clone().strings.as_string);
    EXPECT_FALSE(params->string_info.integers);
  }
  {
    std::optional<choices::ObjectWithChoices::Params> params(
        choices::ObjectWithChoices::Params::Create(
            List(Dictionary("strings", base::Value("asdf"), "integers",
                            base::Value(6)))
                .GetList()));
    ASSERT_TRUE(params);
    EXPECT_FALSE(params->string_info.strings.as_strings);
    EXPECT_EQ("asdf", *params->string_info.strings.as_string);
    EXPECT_EQ("asdf", *params->string_info.Clone().strings.as_string);
    ASSERT_TRUE(params->string_info.integers);
    EXPECT_FALSE(params->string_info.integers->as_integers);
    EXPECT_EQ(6, *params->string_info.integers->as_integer);
    EXPECT_EQ(6, *params->string_info.Clone().integers->as_integer);
  }
}

// TODO(kalman): Clean up the rest of these tests to use the
// Vector/List/Dictionary helpers.

TEST(JsonSchemaCompilerChoicesTest, ObjectWithChoicesParamsCreateFail) {
  {
    base::Value::Dict object_param;
    object_param.Set("strings", 5);
    base::Value::List params_value;
    params_value.Append(std::move(object_param));
    std::optional<choices::ObjectWithChoices::Params> params(
        choices::ObjectWithChoices::Params::Create(
            base::Value(std::move(params_value)).GetList()));
    EXPECT_FALSE(params.has_value());
  }
  {
    base::Value::Dict object_param;
    object_param.Set("strings", "asdf");
    object_param.Set("integers", "asdf");
    base::Value::List params_value;
    params_value.Append(std::move(object_param));
    std::optional<choices::ObjectWithChoices::Params> params(
        choices::ObjectWithChoices::Params::Create(
            base::Value(std::move(params_value)).GetList()));
    EXPECT_FALSE(params.has_value());
  }
  {
    base::Value::Dict object_param;
    object_param.Set("integers", 6);
    base::Value::List params_value;
    params_value.Append(std::move(object_param));
    std::optional<choices::ObjectWithChoices::Params> params(
        choices::ObjectWithChoices::Params::Create(
            base::Value(std::move(params_value)).GetList()));
    EXPECT_FALSE(params.has_value());
  }
}

TEST(JsonSchemaCompilerChoicesTest, PopulateChoiceType) {
  std::vector<std::string> strings = Vector(std::string("list"),
                                            std::string("of"),
                                            std::string("strings"));

  base::Value::List strings_value;
  for (const auto& string : strings)
    strings_value.Append(string);

  base::Value::Dict dict;
  dict.Set("integers", 4);
  dict.Set("strings", std::move(strings_value));

  auto out = choices::ChoiceType::FromValue(dict);
  ASSERT_TRUE(out);

  static_assert(!std::is_copy_constructible_v<choices::ChoiceType>);
  static_assert(!std::is_copy_assignable_v<choices::ChoiceType>);
  static_assert(std::is_move_constructible_v<choices::ChoiceType>);
  static_assert(std::is_move_assignable_v<choices::ChoiceType>);

  ASSERT_TRUE(out->integers.as_integer);
  EXPECT_FALSE(out->integers.as_integers);
  EXPECT_EQ(4, *out->integers.as_integer);

  EXPECT_FALSE(out->strings->as_string);
  ASSERT_TRUE(out->strings->as_strings);
  EXPECT_EQ(strings, *out->strings->as_strings);
}

TEST(JsonSchemaCompilerChoicesTest, ChoiceTypeToValue) {
  base::Value::List strings_value;
  strings_value.Append("list");
  strings_value.Append("of");
  strings_value.Append("strings");

  base::Value::Dict dict;
  dict.Set("integers", 5);
  dict.Set("strings", std::move(strings_value));

  auto out = choices::ChoiceType::FromValue(dict);
  ASSERT_TRUE(out);

  EXPECT_EQ(dict, out->ToValue());
}

TEST(JsonSchemaCompilerChoicesTest, ReturnChoices) {
  {
    choices::ReturnChoices::Results::Result results;
    results.as_integers = Vector<int>(1, 2);

    base::Value results_value(results.ToValue());

    base::Value::List expected;
    expected.Append(1);
    expected.Append(2);

    EXPECT_EQ(expected, results_value);
  }
  {
    choices::ReturnChoices::Results::Result results;
    results.as_integer = 5;

    base::Value results_value(results.ToValue());

    base::Value expected(5);

    EXPECT_EQ(expected, results_value);
  }
}

TEST(JsonSchemaCompilerChoicesTest, NestedChoices) {
  // These test both ToValue and FromValue for every legitimate configuration of
  // NestedChoices.
  {
    // The plain integer choice.
    base::Value value = ReadJson("42");
    std::optional<NestedChoice> obj = NestedChoice::FromValue(value);

    ASSERT_TRUE(obj);
    ASSERT_TRUE(obj->as_integer);
    EXPECT_FALSE(obj->as_choice1);
    EXPECT_FALSE(obj->as_choice2);
    EXPECT_EQ(42, *obj->as_integer);

    EXPECT_EQ(value, obj->ToValue());
  }

  {
    // The string choice within the first choice.
    base::Value value = ReadJson("\"foo\"");
    std::optional<NestedChoice> obj = NestedChoice::FromValue(value);

    ASSERT_TRUE(obj);
    EXPECT_FALSE(obj->as_integer);
    ASSERT_TRUE(obj->as_choice1);
    EXPECT_FALSE(obj->as_choice2);
    ASSERT_TRUE(obj->as_choice1->as_string);
    EXPECT_FALSE(obj->as_choice1->as_boolean);
    EXPECT_EQ("foo", *obj->as_choice1->as_string);

    EXPECT_EQ(value, obj->ToValue());
  }

  {
    // The boolean choice within the first choice.
    base::Value value = ReadJson("true");
    std::optional<NestedChoice> obj = NestedChoice::FromValue(value);

    ASSERT_TRUE(obj);
    EXPECT_FALSE(obj->as_integer);
    ASSERT_TRUE(obj->as_choice1);
    EXPECT_FALSE(obj->as_choice2);
    EXPECT_FALSE(obj->as_choice1->as_string);
    ASSERT_TRUE(obj->as_choice1->as_boolean);
    EXPECT_TRUE(*obj->as_choice1->as_boolean);

    EXPECT_EQ(value, obj->ToValue());
  }

  {
    // The double choice within the second choice.
    base::Value value = ReadJson("42.0");
    std::optional<NestedChoice> obj = NestedChoice::FromValue(value);

    ASSERT_TRUE(obj);
    EXPECT_FALSE(obj->as_integer);
    EXPECT_FALSE(obj->as_choice1);
    ASSERT_TRUE(obj->as_choice2);
    ASSERT_TRUE(obj->as_choice2->as_double_);
    EXPECT_FALSE(obj->as_choice2->as_choice_type);
    EXPECT_FALSE(obj->as_choice2->as_choice_types);
    EXPECT_EQ(42.0, *obj->as_choice2->as_double_);

    EXPECT_EQ(value, obj->ToValue());
  }

  {
    // The ChoiceType choice within the second choice.
    base::Value value =
        ReadJson("{\"integers\": [1, 2], \"strings\": \"foo\"}");
    std::optional<NestedChoice> obj = NestedChoice::FromValue(value);

    ASSERT_TRUE(obj);
    EXPECT_FALSE(obj->as_integer);
    EXPECT_FALSE(obj->as_choice1);
    ASSERT_TRUE(obj->as_choice2);
    EXPECT_FALSE(obj->as_choice2->as_double_);
    ASSERT_TRUE(obj->as_choice2->as_choice_type);
    EXPECT_FALSE(obj->as_choice2->as_choice_types);
    {
      const choices::ChoiceType& choice_type = *obj->as_choice2->as_choice_type;
      ASSERT_TRUE(choice_type.integers.as_integers);
      EXPECT_FALSE(choice_type.integers.as_integer);
      EXPECT_EQ(Vector(1, 2), *choice_type.integers.as_integers);
      ASSERT_TRUE(choice_type.strings);
      EXPECT_FALSE(choice_type.strings->as_strings);
      ASSERT_TRUE(choice_type.strings->as_string);
      EXPECT_EQ("foo", *choice_type.strings->as_string);
    }

    EXPECT_EQ(value, obj->ToValue());
  }

  {
    // The array of ChoiceTypes within the second choice.
    base::Value value = ReadJson(
        "["
        "  {\"integers\": [1, 2], \"strings\": \"foo\"},"
        "  {\"integers\": 3, \"strings\": [\"bar\", \"baz\"]}"
        "]");
    std::optional<NestedChoice> obj = NestedChoice::FromValue(value);

    ASSERT_TRUE(obj);
    EXPECT_FALSE(obj->as_integer);
    EXPECT_FALSE(obj->as_choice1);
    ASSERT_TRUE(obj->as_choice2);
    EXPECT_FALSE(obj->as_choice2->as_double_);
    EXPECT_FALSE(obj->as_choice2->as_choice_type);
    ASSERT_TRUE(obj->as_choice2->as_choice_types);
    // Bleh too much effort to test everything.
    ASSERT_EQ(2u, obj->as_choice2->as_choice_types->size());

    EXPECT_EQ(value, obj->ToValue());
  }
}

}  // namespace
