// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/choices.h"

#include <stddef.h>

#include <utility>

#include "base/strings/string_piece.h"
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
    std::unique_ptr<TakesIntegers::Params> params(TakesIntegers::Params::Create(
        *List(std::make_unique<base::Value>(true))));
    EXPECT_FALSE(params);
  }
  {
    std::unique_ptr<TakesIntegers::Params> params(
        TakesIntegers::Params::Create(*List(std::make_unique<base::Value>(6))));
    ASSERT_TRUE(params);
    EXPECT_FALSE(params->nums.as_integers);
    EXPECT_EQ(6, *params->nums.as_integer);
  }
  {
    std::unique_ptr<TakesIntegers::Params> params(
        TakesIntegers::Params::Create(*List(List(
            std::make_unique<base::Value>(2), std::make_unique<base::Value>(6),
            std::make_unique<base::Value>(8)))));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->nums.as_integers);
    EXPECT_EQ(Vector(2, 6, 8), *params->nums.as_integers);
  }
}

TEST(JsonSchemaCompilerChoicesTest, ObjectWithChoicesParamsCreate) {
  {
    std::unique_ptr<choices::ObjectWithChoices::Params> params(
        choices::ObjectWithChoices::Params::Create(*List(
            Dictionary("strings", std::make_unique<base::Value>("asdf")))));
    ASSERT_TRUE(params);
    EXPECT_FALSE(params->string_info.strings.as_strings);
    EXPECT_EQ("asdf", *params->string_info.strings.as_string);
    EXPECT_FALSE(params->string_info.integers);
  }
  {
    std::unique_ptr<choices::ObjectWithChoices::Params> params(
        choices::ObjectWithChoices::Params::Create(
            *List(Dictionary("strings", std::make_unique<base::Value>("asdf"),
                             "integers", std::make_unique<base::Value>(6)))));
    ASSERT_TRUE(params);
    EXPECT_FALSE(params->string_info.strings.as_strings);
    EXPECT_EQ("asdf", *params->string_info.strings.as_string);
    ASSERT_TRUE(params->string_info.integers);
    EXPECT_FALSE(params->string_info.integers->as_integers);
    EXPECT_EQ(6, *params->string_info.integers->as_integer);
  }
}

// TODO(kalman): Clean up the rest of these tests to use the
// Vector/List/Dictionary helpers.

TEST(JsonSchemaCompilerChoicesTest, ObjectWithChoicesParamsCreateFail) {
  {
    auto object_param = std::make_unique<base::DictionaryValue>();
    object_param->SetKey("strings", base::Value(5));
    std::unique_ptr<base::ListValue> params_value(new base::ListValue());
    params_value->Append(std::move(object_param));
    std::unique_ptr<choices::ObjectWithChoices::Params> params(
        choices::ObjectWithChoices::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
  {
    auto object_param = std::make_unique<base::DictionaryValue>();
    object_param->SetKey("strings", base::Value("asdf"));
    object_param->SetKey("integers", base::Value("asdf"));
    std::unique_ptr<base::ListValue> params_value(new base::ListValue());
    params_value->Append(std::move(object_param));
    std::unique_ptr<choices::ObjectWithChoices::Params> params(
        choices::ObjectWithChoices::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
  {
    auto object_param = std::make_unique<base::DictionaryValue>();
    object_param->SetKey("integers", base::Value(6));
    std::unique_ptr<base::ListValue> params_value(new base::ListValue());
    params_value->Append(std::move(object_param));
    std::unique_ptr<choices::ObjectWithChoices::Params> params(
        choices::ObjectWithChoices::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerChoicesTest, PopulateChoiceType) {
  std::vector<std::string> strings = Vector(std::string("list"),
                                            std::string("of"),
                                            std::string("strings"));

  auto strings_value = std::make_unique<base::ListValue>();
  for (size_t i = 0; i < strings.size(); ++i)
    strings_value->AppendString(strings[i]);

  base::DictionaryValue value;
  value.SetInteger("integers", 4);
  value.Set("strings", std::move(strings_value));

  choices::ChoiceType out;
  ASSERT_TRUE(choices::ChoiceType::Populate(value, &out));
  ASSERT_TRUE(out.integers.as_integer.get());
  EXPECT_FALSE(out.integers.as_integers.get());
  EXPECT_EQ(4, *out.integers.as_integer);

  EXPECT_FALSE(out.strings->as_string.get());
  ASSERT_TRUE(out.strings->as_strings.get());
  EXPECT_EQ(strings, *out.strings->as_strings);
}

TEST(JsonSchemaCompilerChoicesTest, ChoiceTypeToValue) {
  auto strings_value = std::make_unique<base::ListValue>();
  strings_value->AppendString("list");
  strings_value->AppendString("of");
  strings_value->AppendString("strings");

  base::DictionaryValue value;
  value.SetInteger("integers", 5);
  value.Set("strings", std::move(strings_value));

  choices::ChoiceType out;
  ASSERT_TRUE(choices::ChoiceType::Populate(value, &out));

  EXPECT_TRUE(value.Equals(out.ToValue().get()));
}

TEST(JsonSchemaCompilerChoicesTest, ReturnChoices) {
  {
    choices::ReturnChoices::Results::Result results;
    results.as_integers = std::make_unique<std::vector<int>>(Vector(1, 2));

    std::unique_ptr<base::Value> results_value = results.ToValue();
    ASSERT_TRUE(results_value);

    base::ListValue expected;
    expected.AppendInteger(1);
    expected.AppendInteger(2);

    EXPECT_TRUE(expected.Equals(results_value.get()));
  }
  {
    choices::ReturnChoices::Results::Result results;
    results.as_integer = std::make_unique<int>(5);

    std::unique_ptr<base::Value> results_value = results.ToValue();
    ASSERT_TRUE(results_value);

    base::Value expected(5);

    EXPECT_TRUE(expected.Equals(results_value.get()));
  }
}

TEST(JsonSchemaCompilerChoicesTest, NestedChoices) {
  // These test both ToValue and FromValue for every legitimate configuration of
  // NestedChoices.
  {
    // The plain integer choice.
    std::unique_ptr<base::Value> value = ReadJson("42");
    std::unique_ptr<NestedChoice> obj = NestedChoice::FromValue(*value);

    ASSERT_TRUE(obj);
    ASSERT_TRUE(obj->as_integer);
    EXPECT_FALSE(obj->as_choice1);
    EXPECT_FALSE(obj->as_choice2);
    EXPECT_EQ(42, *obj->as_integer);

    EXPECT_EQ(*value, *obj->ToValue());
  }

  {
    // The string choice within the first choice.
    std::unique_ptr<base::Value> value = ReadJson("\"foo\"");
    std::unique_ptr<NestedChoice> obj = NestedChoice::FromValue(*value);

    ASSERT_TRUE(obj);
    EXPECT_FALSE(obj->as_integer);
    ASSERT_TRUE(obj->as_choice1);
    EXPECT_FALSE(obj->as_choice2);
    ASSERT_TRUE(obj->as_choice1->as_string);
    EXPECT_FALSE(obj->as_choice1->as_boolean);
    EXPECT_EQ("foo", *obj->as_choice1->as_string);

    EXPECT_EQ(*value, *obj->ToValue());
  }

  {
    // The boolean choice within the first choice.
    std::unique_ptr<base::Value> value = ReadJson("true");
    std::unique_ptr<NestedChoice> obj = NestedChoice::FromValue(*value);

    ASSERT_TRUE(obj);
    EXPECT_FALSE(obj->as_integer);
    ASSERT_TRUE(obj->as_choice1);
    EXPECT_FALSE(obj->as_choice2);
    EXPECT_FALSE(obj->as_choice1->as_string);
    ASSERT_TRUE(obj->as_choice1->as_boolean);
    EXPECT_TRUE(*obj->as_choice1->as_boolean);

    EXPECT_EQ(*value, *obj->ToValue());
  }

  {
    // The double choice within the second choice.
    std::unique_ptr<base::Value> value = ReadJson("42.0");
    std::unique_ptr<NestedChoice> obj = NestedChoice::FromValue(*value);

    ASSERT_TRUE(obj);
    EXPECT_FALSE(obj->as_integer);
    EXPECT_FALSE(obj->as_choice1);
    ASSERT_TRUE(obj->as_choice2);
    ASSERT_TRUE(obj->as_choice2->as_double);
    EXPECT_FALSE(obj->as_choice2->as_choice_type);
    EXPECT_FALSE(obj->as_choice2->as_choice_types);
    EXPECT_EQ(42.0, *obj->as_choice2->as_double);

    EXPECT_EQ(*value, *obj->ToValue());
  }

  {
    // The ChoiceType choice within the second choice.
    std::unique_ptr<base::Value> value =
        ReadJson("{\"integers\": [1, 2], \"strings\": \"foo\"}");
    std::unique_ptr<NestedChoice> obj = NestedChoice::FromValue(*value);

    ASSERT_TRUE(obj);
    EXPECT_FALSE(obj->as_integer);
    EXPECT_FALSE(obj->as_choice1);
    ASSERT_TRUE(obj->as_choice2);
    EXPECT_FALSE(obj->as_choice2->as_double);
    ASSERT_TRUE(obj->as_choice2->as_choice_type);
    EXPECT_FALSE(obj->as_choice2->as_choice_types);
    {
      choices::ChoiceType* choice_type = obj->as_choice2->as_choice_type.get();
      ASSERT_TRUE(choice_type->integers.as_integers);
      EXPECT_FALSE(choice_type->integers.as_integer);
      EXPECT_EQ(Vector(1, 2), *choice_type->integers.as_integers);
      ASSERT_TRUE(choice_type->strings);
      EXPECT_FALSE(choice_type->strings->as_strings);
      ASSERT_TRUE(choice_type->strings->as_string);
      EXPECT_EQ("foo", *choice_type->strings->as_string);
    }

    EXPECT_EQ(*value, *obj->ToValue());
  }

  {
    // The array of ChoiceTypes within the second choice.
    std::unique_ptr<base::Value> value = ReadJson(
        "["
        "  {\"integers\": [1, 2], \"strings\": \"foo\"},"
        "  {\"integers\": 3, \"strings\": [\"bar\", \"baz\"]}"
        "]");
    std::unique_ptr<NestedChoice> obj = NestedChoice::FromValue(*value);

    ASSERT_TRUE(obj);
    EXPECT_FALSE(obj->as_integer);
    EXPECT_FALSE(obj->as_choice1);
    ASSERT_TRUE(obj->as_choice2);
    EXPECT_FALSE(obj->as_choice2->as_double);
    EXPECT_FALSE(obj->as_choice2->as_choice_type);
    ASSERT_TRUE(obj->as_choice2->as_choice_types);
    // Bleh too much effort to test everything.
    ASSERT_EQ(2u, obj->as_choice2->as_choice_types->size());

    EXPECT_EQ(*value, *obj->ToValue());
  }
}

}  // namespace
