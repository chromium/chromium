// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/idl_basics.h"
#include "tools/json_schema_compiler/test/idl_object_types.h"
#include "tools/json_schema_compiler/test/idl_properties.h"
#include "tools/json_schema_compiler/test/test_util.h"

using test::api::idl_basics::MyType1;
using test::api::idl_object_types::BarType;
using test::api::idl_object_types::FooType;

namespace Function2 = test::api::idl_basics::Function2;
namespace Function3 = test::api::idl_basics::Function3;
namespace Function4 = test::api::idl_basics::Function4;
namespace Function5 = test::api::idl_basics::Function5;
namespace Function6 = test::api::idl_basics::Function6;
namespace Function7 = test::api::idl_basics::Function7;
namespace Function8 = test::api::idl_basics::Function8;
namespace Function9 = test::api::idl_basics::Function9;
namespace Function10 = test::api::idl_basics::Function10;
namespace Function11 = test::api::idl_basics::Function11;
namespace ObjectFunction1 = test::api::idl_object_types::ObjectFunction1;

namespace {

// Parses `idl_basics::ManifestKeys` from the provided `key_values`, using
// stub values for any omitted top-level keys.
bool ParseManifestKeys(const std::string& key_values,
                       test::api::idl_basics::ManifestKeys& manifest_keys,
                       std::u16string& error) {
  // ManifestKeys specify a number of different required keys. In order to allow
  // tests to focus on testing one particular value, rather than all of them,
  // we provide a default "stub" with valid values. Any values passed into
  // `key_values` will overwrite the values in this stub.
  static constexpr char kStubValuesStr[] =
      R"({
           "key_str": "my key",
           "key_ref": {"x": "my ref"},
           "inline_choice": 3,
           "choice_with_arrays": {"entries": "choice"},
           "choice_with_optional": {"entries": "choice"}
         })";
  base::Value::Dict dict_value = base::test::ParseJsonDict(kStubValuesStr);
  base::Value::Dict provided_values = base::test::ParseJsonDict(key_values);

  // Annoying: `base::Value::Dict` values are recursively merged in
  // `base::Value::Dict::Merge()`, rather than overwritten. Remove them from
  // the `dict_value` if a new value was provided for them.
  if (provided_values.contains("choice_with_arrays")) {
    dict_value.Remove("choice_with_arrays");
  }
  if (provided_values.contains("choice_with_optional")) {
    dict_value.Remove("choice_with_optional");
  }
  dict_value.Merge(std::move(provided_values));

  return test::api::idl_basics::ManifestKeys::ParseFromDictionary(
      dict_value, manifest_keys, error);
}

// Parses `idl_basics::ManifestKeys` from the provided `key_values`, expecting
// success and returning the parsed value.
test::api::idl_basics::ManifestKeys ParseManifestKeysAndReturnValue(
    const std::string& key_values) {
  test::api::idl_basics::ManifestKeys manifest_keys;
  std::u16string error;
  bool result = ParseManifestKeys(key_values, manifest_keys, error);
  EXPECT_TRUE(result) << "Parsing failed.\nValue:\n"
                      << key_values << "\nError: " << base::UTF16ToUTF8(error);

  return manifest_keys;
}

// Attempts to parse `idl_basics::ManifestKeys` from the provided `key_values`,
// expecting failure and returning the encountered parse error.
std::string ParseManifestKeysAndReturnError(const std::string& key_values) {
  test::api::idl_basics::ManifestKeys manifest_keys;
  std::u16string error;
  bool result = ParseManifestKeys(key_values, manifest_keys, error);
  EXPECT_FALSE(result) << "Parsing unexpected succeeded";
  return result ? "<no error>" : base::UTF16ToUTF8(error);
}

}  // namespace

TEST(IdlCompiler, Basics) {
  // Test MyType1.
  static_assert(!std::is_copy_constructible_v<MyType1>);
  static_assert(!std::is_copy_assignable_v<MyType1>);
  static_assert(std::is_move_constructible_v<MyType1>);
  static_assert(std::is_move_assignable_v<MyType1>);

  MyType1 a;
  a.x = 5;
  a.y = std::string("foo");
  auto b = MyType1::FromValue(a.ToValue());
  ASSERT_TRUE(b);
  EXPECT_EQ(a.x, b->x);
  EXPECT_EQ(a.y, b->y);
  EXPECT_EQ(a.Clone().ToValue(), a.ToValue());

  // Test Function2, which takes an integer parameter.
  base::Value::List list;
  list.Append(5);
  std::optional<Function2::Params> f2_params = Function2::Params::Create(list);
  EXPECT_EQ(5, f2_params->x);

  // Test Function3, which takes a MyType1 parameter.
  list.clear();
  base::Value::Dict tmp;
  tmp.Set("x", 17);
  tmp.Set("y", "hello");
  tmp.Set("z", "zstring");
  tmp.Set("a", "astring");
  tmp.Set("b", "bstring");
  tmp.Set("c", "cstring");
  list.Append(base::Value(std::move(tmp)));
  std::optional<Function3::Params> f3_params = Function3::Params::Create(list);
  EXPECT_EQ(17, f3_params->arg.x);
  EXPECT_EQ("hello", f3_params->arg.y);

  // Test functions that take a callback function as a parameter, with varying
  // callback signatures.
  base::Value::List f4_results = Function4::Results::Create();
  base::Value::List expected;
  EXPECT_EQ(expected, f4_results);

  base::Value::List f5_results = Function5::Results::Create(13);
  ASSERT_EQ(1u, f5_results.size());
  EXPECT_TRUE(f5_results[0].is_int());

  base::Value::List f6_results = Function6::Results::Create(a);
  ASSERT_EQ(1u, f6_results.size());
  auto c = MyType1::FromValue(f6_results[0]);
  ASSERT_TRUE(c);
  EXPECT_EQ(a.x, c->x);
  EXPECT_EQ(a.y, c->y);
}

TEST(IdlCompiler, OptionalArguments) {
  // Test a function that takes one optional argument, both without and with
  // that argument.
  base::Value::List list;
  std::optional<Function7::Params> f7_params = Function7::Params::Create(list);
  EXPECT_FALSE(f7_params->arg.has_value());
  list.Append(7);
  f7_params = Function7::Params::Create(list);
  EXPECT_EQ(7, *(f7_params->arg));

  // Similar to above, but a function with one required and one optional
  // argument.
  list.clear();
  list.Append(8);
  std::optional<Function8::Params> f8_params = Function8::Params::Create(list);
  EXPECT_EQ(8, f8_params->arg1);
  EXPECT_FALSE(f8_params->arg2.has_value());
  list.Append("foo");
  f8_params = Function8::Params::Create(list);
  EXPECT_EQ(8, f8_params->arg1);
  EXPECT_EQ("foo", *(f8_params->arg2));

  // Test a function with an optional argument of custom type.
  list.clear();
  std::optional<Function9::Params> f9_params = Function9::Params::Create(list);
  EXPECT_FALSE(f9_params->arg.has_value());
  list.clear();
  base::Value::Dict tmp;
  tmp.Set("x", 17);
  tmp.Set("y", "hello");
  tmp.Set("z", "zstring");
  tmp.Set("a", "astring");
  tmp.Set("b", "bstring");
  tmp.Set("c", "cstring");
  list.Append(base::Value(std::move(tmp)));
  f9_params = Function9::Params::Create(list);
  ASSERT_TRUE(f9_params->arg);
  const MyType1& t1 = *f9_params->arg;
  EXPECT_EQ(17, t1.x);
  EXPECT_EQ("hello", t1.y);
}

TEST(IdlCompiler, ArrayTypes) {
  // Tests of a function that takes an integer and an array of integers. First
  // use an empty array.
  base::Value::List list;
  list.Append(33);
  list.Append(base::Value::List());
  std::optional<Function10::Params> f10_params =
      Function10::Params::Create(list);
  ASSERT_TRUE(f10_params != std::nullopt);
  EXPECT_EQ(33, f10_params->x);
  EXPECT_TRUE(f10_params->y.empty());

  // Same function, but this time with 2 values in the array.
  list.clear();
  list.Append(33);
  base::Value::List sublist;
  sublist.Append(34);
  sublist.Append(35);
  list.Append(std::move(sublist));
  f10_params = Function10::Params::Create(list);
  ASSERT_TRUE(f10_params != std::nullopt);
  EXPECT_EQ(33, f10_params->x);
  ASSERT_EQ(2u, f10_params->y.size());
  EXPECT_EQ(34, f10_params->y[0]);
  EXPECT_EQ(35, f10_params->y[1]);

  // Now test a function which takes an array of a defined type.
  list.clear();
  MyType1 a;
  MyType1 b;
  a.x = 5;
  b.x = 6;
  a.y = std::string("foo");
  b.y = std::string("bar");
  base::Value::List sublist2;
  sublist2.Append(base::Value(a.ToValue()));
  sublist2.Append(base::Value(b.ToValue()));
  list.Append(std::move(sublist2));
  std::optional<Function11::Params> f11_params =
      Function11::Params::Create(list);
  ASSERT_TRUE(f11_params != std::nullopt);
  ASSERT_EQ(2u, f11_params->arg.size());
  EXPECT_EQ(5, f11_params->arg[0].x);
  EXPECT_EQ("foo", f11_params->arg[0].y);
  EXPECT_EQ(6, f11_params->arg[1].x);
  EXPECT_EQ("bar", f11_params->arg[1].y);
}

TEST(IdlCompiler, ObjectTypes) {
  // Test the FooType type.
  FooType f1;
  f1.x = 3;
  auto f2 = FooType::FromValue(f1.ToValue());
  ASSERT_TRUE(f2);
  EXPECT_EQ(f1.x, f2->x);

  // Test the BarType type.
  BarType b1;
  b1.x = base::Value(7);
  auto b2 = BarType::FromValue(b1.ToValue());
  ASSERT_TRUE(b2);
  ASSERT_TRUE(b2->x.is_int());
  EXPECT_EQ(7, b2->x.GetInt());
  EXPECT_FALSE(b2->y.has_value());

  // Test the params to the ObjectFunction1 function.
  base::Value::Dict icon_props;
  icon_props.Set("hello", "world");
  ASSERT_TRUE(ObjectFunction1::Params::Icon::FromValue(icon_props));
  base::Value::List list;
  list.Append(std::move(icon_props));
  std::optional<ObjectFunction1::Params> params =
      ObjectFunction1::Params::Create(list);
  ASSERT_TRUE(params != std::nullopt);
  const std::string* tmp =
      params->icon.additional_properties.FindString("hello");
  ASSERT_TRUE(tmp);
  EXPECT_EQ("world", *tmp);
}

// Tests using IDL "choices" in manifest key-specified types.
TEST(IdlCompiler, ManifestKeys_Choices) {
  {
    // String entry for an inline choice.
    static constexpr char kManifestKeys[] =
        R"({"inline_choice": "string value"})";
    test::api::idl_basics::ManifestKeys manifest_keys =
        ParseManifestKeysAndReturnValue(kManifestKeys);
    EXPECT_EQ("string value", manifest_keys.inline_choice.as_string);
    EXPECT_EQ(std::nullopt, manifest_keys.inline_choice.as_integer);
  }

  {
    // Single entry for non-optional choices.
    static constexpr char kManifestKeys[] =
        R"({"choice_with_arrays": {"entries": "single entry"}})";
    test::api::idl_basics::ManifestKeys manifest_keys =
        ParseManifestKeysAndReturnValue(kManifestKeys);
    EXPECT_EQ("single entry",
              manifest_keys.choice_with_arrays.entries.as_string);
    EXPECT_EQ(std::nullopt,
              manifest_keys.choice_with_arrays.entries.as_strings);
  }

  {
    // Single entry for optional choices.
    static constexpr char kManifestKeys[] =
        R"({"choice_with_optional": {"entries": "single entry"}})";
    test::api::idl_basics::ManifestKeys manifest_keys =
        ParseManifestKeysAndReturnValue(kManifestKeys);
    ASSERT_TRUE(manifest_keys.choice_with_optional.entries);
    EXPECT_EQ("single entry",
              manifest_keys.choice_with_optional.entries->as_string);
    EXPECT_EQ(std::nullopt,
              manifest_keys.choice_with_optional.entries->as_strings);
  }

  {
    // Integer entry for an inline choice.
    static constexpr char kManifestKeys[] = R"({"inline_choice": 42})";
    test::api::idl_basics::ManifestKeys manifest_keys =
        ParseManifestKeysAndReturnValue(kManifestKeys);
    EXPECT_EQ(std::nullopt, manifest_keys.inline_choice.as_string);
    EXPECT_EQ(42, manifest_keys.inline_choice.as_integer);
  }

  {
    // List entry for non-optional choices.
    static constexpr char kManifestKeys[] =
        R"({"choice_with_arrays": {"entries": ["entry1", "entry2"]}})";
    test::api::idl_basics::ManifestKeys manifest_keys =
        ParseManifestKeysAndReturnValue(kManifestKeys);
    EXPECT_EQ(std::nullopt, manifest_keys.choice_with_arrays.entries.as_string);
    EXPECT_THAT(*manifest_keys.choice_with_arrays.entries.as_strings,
                testing::ElementsAre("entry1", "entry2"));
  }

  {
    // List entry for optional choices.
    static constexpr char kManifestKeys[] =
        R"({"choice_with_optional": {"entries": ["entry1", "entry2"]}})";
    test::api::idl_basics::ManifestKeys manifest_keys =
        ParseManifestKeysAndReturnValue(kManifestKeys);
    ASSERT_TRUE(manifest_keys.choice_with_optional.entries);
    EXPECT_EQ(std::nullopt,
              manifest_keys.choice_with_optional.entries->as_string);
    EXPECT_THAT(*manifest_keys.choice_with_optional.entries->as_strings,
                testing::ElementsAre("entry1", "entry2"));
  }

  {
    // Omitted optional choice.
    static constexpr char kManifestKeys[] = R"({"choice_with_optional": { }})";
    test::api::idl_basics::ManifestKeys manifest_keys =
        ParseManifestKeysAndReturnValue(kManifestKeys);
    EXPECT_EQ(std::nullopt, manifest_keys.choice_with_optional.entries);
  }

  {
    // Invalid entry for an inline choice.
    static constexpr char kManifestKeys[] = R"({"inline_choice": ["a", "b"]})";
    EXPECT_EQ(
        "Error at key 'inline_choice'. "
        "Provided value matches none of the allowed options.",
        ParseManifestKeysAndReturnError(kManifestKeys));
  }

  {
    // Error: Invalid entry for `choice_with_arrays`.
    static constexpr char kManifestKeys[] =
        R"({"choice_with_arrays": {"entries": 3}})";

    EXPECT_EQ(
        "Error at key 'choice_with_arrays.entries'. "
        "Provided value matches none of the allowed options.",
        ParseManifestKeysAndReturnError(kManifestKeys));
  }

  {
    // Error: Invalid entry for `choice_with_optional`.
    static constexpr char kManifestKeys[] =
        R"({"choice_with_optional": {"entries": 3}})";

    EXPECT_EQ(
        "Error at key 'choice_with_optional.entries'. "
        "Provided value matches none of the allowed options.",
        ParseManifestKeysAndReturnError(kManifestKeys));
  }

  {
    // Error: Omitted non-optional choices value.
    static constexpr char kManifestKeys[] = R"({"choice_with_arrays": { }})";

    EXPECT_EQ(
        "Error at key 'choice_with_arrays.entries'. Manifest key is required.",
        ParseManifestKeysAndReturnError(kManifestKeys));
  }
}

TEST(IdlCompiler, PropertyValues) {
  EXPECT_EQ(42, test::api::idl_properties::first);
  EXPECT_EQ(42.1, test::api::idl_properties::second);
  EXPECT_STREQ("hello world", test::api::idl_properties::third);
}
