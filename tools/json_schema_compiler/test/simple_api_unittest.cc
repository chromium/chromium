// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/simple_api.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/enums.h"

namespace simple_api = test::api::simple_api;
namespace enums = test::api::enums;

namespace {

static base::Value::Dict CreateTestTypeDictionary() {
  base::Value::Dict dict;
  dict.Set("number", 1.1);
  dict.Set("integer", 4);
  dict.Set("string", "bling");
  dict.Set("boolean", true);
  return dict;
}

void GetManifestParseError(std::string_view manifest_json, std::string* error) {
  std::optional<base::Value> manifest = base::JSONReader::Read(manifest_json);
  ASSERT_TRUE(manifest) << "Invalid json \n" << manifest_json;

  simple_api::ManifestKeys manifest_keys;
  std::u16string error_16;
  bool result = simple_api::ManifestKeys::ParseFromDictionary(
      manifest->GetDict(), manifest_keys, error_16);

  ASSERT_FALSE(result);
  *error = base::UTF16ToASCII(error_16);
}

void PopulateManifestKeys(std::string_view manifest_json,
                          simple_api::ManifestKeys* manifest_keys) {
  std::optional<base::Value> manifest = base::JSONReader::Read(manifest_json);
  ASSERT_TRUE(manifest.has_value());

  std::u16string error_16;
  bool result = simple_api::ManifestKeys::ParseFromDictionary(
      manifest->GetDict(), *manifest_keys, error_16);

  ASSERT_TRUE(result) << error_16;
  ASSERT_TRUE(error_16.empty()) << error_16;
}

}  // namespace

TEST(JsonSchemaCompilerSimpleTest, IncrementIntegerResultCreate) {
  base::Value results(simple_api::IncrementInteger::Results::Create(5));
  base::Value::List expected;
  expected.Append(5);
  EXPECT_EQ(expected, results);
}

TEST(JsonSchemaCompilerSimpleTest, IncrementIntegerParamsCreate) {
  base::Value::List params_value;
  params_value.Append(6);
  std::optional<simple_api::IncrementInteger::Params> params(
      simple_api::IncrementInteger::Params::Create(params_value));
  EXPECT_TRUE(params.has_value());
  EXPECT_EQ(6, params->num);
}

TEST(JsonSchemaCompilerSimpleTest, NumberOfParams) {
  {
    base::Value::List params_value;
    params_value.Append("text");
    params_value.Append("text");
    std::optional<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(params_value));
    EXPECT_FALSE(params.has_value());
  }
  {
    base::Value::List params_value;
    std::optional<simple_api::IncrementInteger::Params> params(
        simple_api::IncrementInteger::Params::Create(params_value));
    EXPECT_FALSE(params.has_value());
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalStringParamsCreate) {
  {
    base::Value::List params_value;
    std::optional<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_FALSE(params->str);
  }
  {
    base::Value::List params_value;
    params_value.Append("asdf");
    std::optional<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_TRUE(params->str);
    EXPECT_EQ("asdf", *params->str);
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalParamsTakingNull) {
  {
    base::Value::List params_value;
    params_value.Append(base::Value());
    std::optional<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_FALSE(params->str);
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalStringParamsWrongType) {
  {
    base::Value::List params_value;
    params_value.Append(5);
    std::optional<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(params_value));
    EXPECT_FALSE(params.has_value());
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalBeforeRequired) {
  {
    base::Value::List params_value;
    params_value.Append(base::Value());
    params_value.Append("asdf");
    std::optional<simple_api::OptionalBeforeRequired::Params> params(
        simple_api::OptionalBeforeRequired::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_FALSE(params->first);
    EXPECT_EQ("asdf", params->second);
  }
}

TEST(JsonSchemaCompilerSimpleTest, RequiredFunctionParameter) {
  {
    base::Value::List params_value;
    params_value.Append(base::Value::Dict());
    params_value.Append("asdf");
    std::optional<simple_api::RequiredFunctionParameter::Params> params(
        simple_api::RequiredFunctionParameter::Params::Create(params_value));
    EXPECT_TRUE(params.has_value());
    EXPECT_TRUE(params->function_parameter.empty());
    EXPECT_EQ("asdf", params->second);
  }
  {
    base::Value::List params_value;
    params_value.Append(5);
    params_value.Append("asdf");
    std::optional<simple_api::RequiredFunctionParameter::Params> params(
        simple_api::RequiredFunctionParameter::Params::Create(params_value));
    EXPECT_FALSE(params.has_value());
  }
}

TEST(JsonSchemaCompilerSimpleTest, NoParamsResultCreate) {
  base::Value results(simple_api::OptionalString::Results::Create());
  base::Value::List expected;
  EXPECT_EQ(expected, results);
}

TEST(JsonSchemaCompilerSimpleTest, TestTypePopulate) {
  {
    base::Value::Dict value = CreateTestTypeDictionary();
    auto test_type = simple_api::TestType::FromValue(value);
    EXPECT_TRUE(test_type);
    EXPECT_EQ("bling", test_type->string);
    EXPECT_EQ(1.1, test_type->number);
    EXPECT_EQ(4, test_type->integer);
    EXPECT_EQ(true, test_type->boolean);
    EXPECT_EQ(value, test_type->ToValue());
  }
  {
    base::Value::Dict value = CreateTestTypeDictionary();
    value.Remove("number");
    EXPECT_FALSE(simple_api::TestType::FromValue(std::move(value)));
  }
}

TEST(JsonSchemaCompilerSimpleTest, GetTestType) {
  {
    base::Value::Dict value = CreateTestTypeDictionary();
    auto test_type = simple_api::TestType::FromValue(value.Clone());
    ASSERT_TRUE(test_type);
    base::Value::List results =
        simple_api::GetTestType::Results::Create(*test_type);
    ASSERT_EQ(1u, results.size());
    EXPECT_EQ(results[0], value);
  }
}

TEST(JsonSchemaCompilerSimpleTest, OnIntegerFiredCreate) {
  {
    base::Value results(simple_api::OnIntegerFired::Create(5));
    base::Value::List expected;
    expected.Append(5);
    EXPECT_EQ(expected, results);
  }
}

TEST(JsonSchemaCompilerSimpleTest, OnStringFiredCreate) {
  {
    base::Value results(simple_api::OnStringFired::Create("yo dawg"));
    base::Value::List expected;
    expected.Append("yo dawg");
    EXPECT_EQ(expected, results);
  }
}

TEST(JsonSchemaCompilerSimpleTest, OnTestTypeFiredCreate) {
  {
    simple_api::TestType some_test_type;
    base::Value::Dict expected = CreateTestTypeDictionary();

    std::optional<double> number_value = expected.FindDouble("number");
    ASSERT_TRUE(number_value);
    some_test_type.number = *number_value;

    const std::string* string_value = expected.FindString("string");
    ASSERT_TRUE(string_value);
    some_test_type.string = *string_value;

    std::optional<int> int_value = expected.FindInt("integer");
    ASSERT_TRUE(int_value);
    some_test_type.integer = *int_value;

    std::optional<bool> boolean_value = expected.FindBool("boolean");
    ASSERT_TRUE(boolean_value);
    some_test_type.boolean = *boolean_value;

    base::Value results(simple_api::OnTestTypeFired::Create(some_test_type));
    ASSERT_TRUE(results.is_list());
    ASSERT_EQ(1u, results.GetList().size());
    EXPECT_EQ(expected, results.GetList()[0]);
  }
}

TEST(JsonSchemaCompilerSimpleTest, ManifestKeyParsing_RequiredKeyError) {
  const char kPartialManifestJson[] = R"({
    "key_string": "abc",
    "key_ref": {
      "string": "ref_string",
      "boolean": true,
      "number": 25.4
    }
  })";

  std::string error;
  ASSERT_NO_FATAL_FAILURE(GetManifestParseError(kPartialManifestJson, &error));
  EXPECT_EQ("Error at key 'key_ref.integer'. Manifest key is required.", error);
}

TEST(JsonSchemaCompilerSimpleTest, ManifestKeyParsing_InvalidTypeError) {
  const char kPartialManifestJson[] = R"({
    "key_string": "abc",
    "key_ref": {
      "string": "ref_string",
      "boolean": true,
      "number": 25.4,
      "integer": 32,
      "object": {
        "foo": "bar"
      },
      "key_enum": "one",
      "key_enum_array": ["two"]
    }
  })";

  std::string error;
  ASSERT_NO_FATAL_FAILURE(GetManifestParseError(kPartialManifestJson, &error));
  EXPECT_EQ(
      "Error at key 'key_ref.object.foo'. Type is invalid. Expected integer, "
      "found string.",
      error);
}

TEST(JsonSchemaCompilerSimpleTest, ManifestKeyParsing_ArrayParseError) {
  const char kPartialManifestJson[] = R"({
    "key_string": "abc",
    "key_ref": {
      "string": "ref_string",
      "boolean": true,
      "number": 25.4,
      "integer": 32,
      "array": ["one", "two", 3]
    },
    "key_enum": "one",
    "key_enum_array": ["two"]
  })";

  std::string error;
  ASSERT_NO_FATAL_FAILURE(GetManifestParseError(kPartialManifestJson, &error));
  EXPECT_EQ(
      "Error at key 'key_ref.array'. Parsing array failed at index 2: expected "
      "string, got integer",
      error);
}

TEST(JsonSchemaCompilerSimpleTest, ManifestKeyParsing_EnumArrayParseError) {
  {
    const char kPartialManifestJson[] = R"({
      "key_string": "abc",
      "key_ref": {
        "string": "ref_string",
        "boolean": true,
        "number": 25.4,
        "integer": 32,
        "array": ["one", "two"]
      },
      "key_enum": "one",
      "key_enum_array": ["two", false]
    })";

    std::string error;
    ASSERT_NO_FATAL_FAILURE(
        GetManifestParseError(kPartialManifestJson, &error));
    EXPECT_EQ(
        "Error at key 'key_enum_array'. Parsing array failed at index 1: "
        "expected string, got boolean",
        error);
  }
  {
    const char kPartialManifestJson[] = R"({
      "key_string": "abc",
      "key_ref": {
        "string": "ref_string",
        "boolean": true,
        "number": 25.4,
        "integer": 32,
        "array": ["one", "two"]
      },
      "key_enum": "one",
      "key_enum_array": [],
      "key_obj": {
        "obj_string": "foo",
        "obj_bool": true,
        "obj_optional_enum_array": ["one", "invalid_value"]
      }
    })";

    std::string error;
    ASSERT_NO_FATAL_FAILURE(
        GetManifestParseError(kPartialManifestJson, &error));
    EXPECT_EQ(
        "Error at key 'key_obj.obj_optional_enum_array'. Parsing array failed "
        "at index 1: "
        "Specified value 'invalid_value' is invalid.",
        error);
  }
  {
    const char kPartialManifestJson[] = R"({
      "key_string": "abc",
      "key_ref": {
        "string": "ref_string",
        "boolean": true,
        "number": 25.4,
        "integer": 32,
        "array": ["one", "two"]
      },
      "key_enum": "one",
      "key_enum_array": [],
      "key_obj": {
        "obj_string": "foo",
        "obj_bool": true,
        "obj_optional_enum_array": false
      }
    })";

    std::string error;
    ASSERT_NO_FATAL_FAILURE(
        GetManifestParseError(kPartialManifestJson, &error));
    EXPECT_EQ(
        "Error at key 'key_obj.obj_optional_enum_array'. Type is invalid. "
        "Expected list, found boolean.",
        error);
  }
}

TEST(JsonSchemaCompilerSimpleTest,
     ManifestKeyParsing_OptionalEnumArrayParseError) {}

TEST(JsonSchemaCompilerSimpleTest, ManifestKeyParsing_InvalidEnumValue) {
  const char kPartialManifestJson[] = R"({
    "key_string": "abc",
    "key_ref": {
      "string": "ref_string",
      "boolean": true,
      "number": 25.4,
      "integer": 32,
      "opt_external_enum": "four"
    },
    "key_enum": "one",
    "key_enum_array": ["two"]
  })";

  std::string error;
  ASSERT_NO_FATAL_FAILURE(GetManifestParseError(kPartialManifestJson, &error));
  EXPECT_EQ(
      "Error at key 'key_ref.opt_external_enum'. Specified value 'four' is "
      "invalid.",
      error);
}

TEST(JsonSchemaCompilerSimpleTest, ManifestKeyParsing_Success_AllKeys) {
  const char kPartialManifestJson[] = R"({
    "key_string": "abc",
    "key_ref": {
      "string": "ref_string",
      "boolean": true,
      "number": 25.4,
      "integer": 32,
      "object": {
        "foo": 42
      },
      "array": ["one", "two"],
      "opt_external_enum": "two"
    },
    "key_obj": {
      "obj_string": "foo",
      "obj_bool": true,
      "obj_optional_enum_array": ["three"]
    },
    "key_enum": "one",
    "key_enum_array": ["two", "one"],
    "3d_key": "yes"
  })";

  simple_api::ManifestKeys manifest_keys;
  ASSERT_NO_FATAL_FAILURE(
      PopulateManifestKeys(kPartialManifestJson, &manifest_keys));

  EXPECT_EQ("abc", manifest_keys.key_string);

  ASSERT_TRUE(manifest_keys.key_obj);
  EXPECT_EQ("foo", manifest_keys.key_obj->obj_string);
  EXPECT_TRUE(manifest_keys.key_obj->obj_bool);
  ASSERT_TRUE(manifest_keys.key_obj->obj_optional_enum_array);
  EXPECT_THAT(*manifest_keys.key_obj->obj_optional_enum_array,
              ::testing::ElementsAre(enums::Enumeration::kThree));

  EXPECT_EQ(simple_api::TestEnum::kOne, manifest_keys.key_enum);

  EXPECT_EQ("ref_string", manifest_keys.key_ref.string);
  EXPECT_EQ(true, manifest_keys.key_ref.boolean);
  EXPECT_DOUBLE_EQ(25.4, manifest_keys.key_ref.number);
  EXPECT_EQ(32, manifest_keys.key_ref.integer);
  ASSERT_TRUE(manifest_keys.key_ref.object);
  EXPECT_EQ(42, manifest_keys.key_ref.object->foo);
  ASSERT_TRUE(manifest_keys.key_ref.array);
  EXPECT_THAT(*manifest_keys.key_ref.array,
              ::testing::ElementsAre("one", "two"));
  EXPECT_EQ(enums::Enumeration::kTwo, manifest_keys.key_ref.opt_external_enum);
  EXPECT_THAT(manifest_keys.key_enum_array,
              ::testing::ElementsAre(simple_api::TestEnum::kTwo,
                                     simple_api::TestEnum::kOne));
  EXPECT_EQ(simple_api::_3D::kYes, manifest_keys._3d_key);
}

// Ensure leaving out optional keys is not a manifest parse error.
TEST(JsonSchemaCompilerSimpleTest,
     ManifestKeyParsing_Success_OptionalKeysIgnored) {
  const char kPartialManifestJson[] = R"({
    "key_string": "abc",
    "key_ref": {
      "string": "ref_string",
      "boolean": true,
      "number": 25.4,
      "integer": 32
    },
    "key_enum": "two",
    "key_enum_array": ["one"]
  })";

  simple_api::ManifestKeys manifest_keys;
  ASSERT_NO_FATAL_FAILURE(
      PopulateManifestKeys(kPartialManifestJson, &manifest_keys));

  EXPECT_EQ("abc", manifest_keys.key_string);
  EXPECT_FALSE(manifest_keys.key_obj);
  EXPECT_EQ(simple_api::TestEnum::kTwo, manifest_keys.key_enum);

  EXPECT_EQ("ref_string", manifest_keys.key_ref.string);
  EXPECT_EQ(true, manifest_keys.key_ref.boolean);
  EXPECT_DOUBLE_EQ(25.4, manifest_keys.key_ref.number);
  EXPECT_EQ(32, manifest_keys.key_ref.integer);
  EXPECT_FALSE(manifest_keys.key_ref.array);
  EXPECT_EQ(enums::Enumeration::kNone, manifest_keys.key_ref.opt_external_enum);
  EXPECT_EQ(simple_api::_3D::kNone, manifest_keys._3d_key);
}
