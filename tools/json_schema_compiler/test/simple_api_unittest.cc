// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/simple_api.h"

#include <memory>
#include <string>

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

static std::unique_ptr<base::DictionaryValue> CreateTestTypeDictionary() {
  auto value = std::make_unique<base::DictionaryValue>();
  value->SetKey("number", base::Value(1.1));
  value->SetKey("integer", base::Value(4));
  value->SetKey("string", base::Value("bling"));
  value->SetKey("boolean", base::Value(true));
  return value;
}

void GetManifestParseError(base::StringPiece manifest_json,
                           std::string* error) {
  base::Optional<base::Value> manifest = base::JSONReader::Read(manifest_json);
  ASSERT_TRUE(manifest);

  simple_api::ManifestKeys manifest_keys;
  base::string16 error_16;
  bool result = simple_api::ManifestKeys::ParseFromDictionary(
      base::Value::AsDictionaryValue(*manifest), &manifest_keys, &error_16);

  ASSERT_FALSE(result);
  *error = base::UTF16ToASCII(error_16);
}

void PopulateManifestKeys(base::StringPiece manifest_json,
                          simple_api::ManifestKeys* manifest_keys) {
  base::Optional<base::Value> manifest = base::JSONReader::Read(manifest_json);
  ASSERT_TRUE(manifest.has_value());

  base::string16 error_16;
  bool result = simple_api::ManifestKeys::ParseFromDictionary(
      base::Value::AsDictionaryValue(*manifest), manifest_keys, &error_16);

  ASSERT_TRUE(result) << error_16;
  ASSERT_TRUE(error_16.empty()) << error_16;
}

}  // namespace

TEST(JsonSchemaCompilerSimpleTest, IncrementIntegerResultCreate) {
  base::Value results = base::Value::FromUniquePtrValue(
      simple_api::IncrementInteger::Results::Create(5));
  base::ListValue expected;
  expected.AppendInteger(5);
  EXPECT_EQ(expected, results);
}

TEST(JsonSchemaCompilerSimpleTest, IncrementIntegerParamsCreate) {
  auto params_value = std::make_unique<base::ListValue>();
  params_value->AppendInteger(6);
  std::unique_ptr<simple_api::IncrementInteger::Params> params(
      simple_api::IncrementInteger::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  EXPECT_EQ(6, params->num);
}

TEST(JsonSchemaCompilerSimpleTest, NumberOfParams) {
  {
    auto params_value = std::make_unique<base::ListValue>();
    params_value->AppendString("text");
    params_value->AppendString("text");
    std::unique_ptr<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
  {
    auto params_value = std::make_unique<base::ListValue>();
    std::unique_ptr<simple_api::IncrementInteger::Params> params(
        simple_api::IncrementInteger::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalStringParamsCreate) {
  {
    auto params_value = std::make_unique<base::ListValue>();
    std::unique_ptr<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_FALSE(params->str.get());
  }
  {
    auto params_value = std::make_unique<base::ListValue>();
    params_value->AppendString("asdf");
    std::unique_ptr<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_TRUE(params->str.get());
    EXPECT_EQ("asdf", *params->str);
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalParamsTakingNull) {
  {
    auto params_value = std::make_unique<base::ListValue>();
    params_value->Append(std::make_unique<base::Value>());
    std::unique_ptr<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_FALSE(params->str.get());
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalStringParamsWrongType) {
  {
    auto params_value = std::make_unique<base::ListValue>();
    params_value->AppendInteger(5);
    std::unique_ptr<simple_api::OptionalString::Params> params(
        simple_api::OptionalString::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalBeforeRequired) {
  {
    auto params_value = std::make_unique<base::ListValue>();
    params_value->Append(std::make_unique<base::Value>());
    params_value->AppendString("asdf");
    std::unique_ptr<simple_api::OptionalBeforeRequired::Params> params(
        simple_api::OptionalBeforeRequired::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_FALSE(params->first.get());
    EXPECT_EQ("asdf", params->second);
  }
}

TEST(JsonSchemaCompilerSimpleTest, NoParamsResultCreate) {
  base::Value results = base::Value::FromUniquePtrValue(
      simple_api::OptionalString::Results::Create());
  base::ListValue expected;
  EXPECT_EQ(expected, results);
}

TEST(JsonSchemaCompilerSimpleTest, TestTypePopulate) {
  {
    auto test_type = std::make_unique<simple_api::TestType>();
    std::unique_ptr<base::DictionaryValue> value = CreateTestTypeDictionary();
    EXPECT_TRUE(simple_api::TestType::Populate(*value, test_type.get()));
    EXPECT_EQ("bling", test_type->string);
    EXPECT_EQ(1.1, test_type->number);
    EXPECT_EQ(4, test_type->integer);
    EXPECT_EQ(true, test_type->boolean);
    EXPECT_TRUE(value->Equals(test_type->ToValue().get()));
  }
  {
    auto test_type = std::make_unique<simple_api::TestType>();
    std::unique_ptr<base::DictionaryValue> value = CreateTestTypeDictionary();
    value->Remove("number", NULL);
    EXPECT_FALSE(simple_api::TestType::Populate(*value, test_type.get()));
  }
}

TEST(JsonSchemaCompilerSimpleTest, GetTestType) {
  {
    std::unique_ptr<base::DictionaryValue> value = CreateTestTypeDictionary();
    auto test_type = std::make_unique<simple_api::TestType>();
    EXPECT_TRUE(simple_api::TestType::Populate(*value, test_type.get()));
    base::Value results = base::Value::FromUniquePtrValue(
        simple_api::GetTestType::Results::Create(*test_type));
    ASSERT_TRUE(results.is_list());
    ASSERT_EQ(1u, results.GetList().size());
    EXPECT_TRUE(results.GetList()[0].Equals(value.get()));
  }
}

TEST(JsonSchemaCompilerSimpleTest, OnIntegerFiredCreate) {
  {
    base::Value results =
        base::Value::FromUniquePtrValue(simple_api::OnIntegerFired::Create(5));
    base::ListValue expected;
    expected.AppendInteger(5);
    EXPECT_EQ(expected, results);
  }
}

TEST(JsonSchemaCompilerSimpleTest, OnStringFiredCreate) {
  {
    base::Value results = base::Value::FromUniquePtrValue(
        simple_api::OnStringFired::Create("yo dawg"));
    base::ListValue expected;
    expected.AppendString("yo dawg");
    EXPECT_EQ(expected, results);
  }
}

TEST(JsonSchemaCompilerSimpleTest, OnTestTypeFiredCreate) {
  {
    simple_api::TestType some_test_type;
    std::unique_ptr<base::DictionaryValue> expected =
        CreateTestTypeDictionary();
    ASSERT_TRUE(expected->GetDouble("number", &some_test_type.number));
    ASSERT_TRUE(expected->GetString("string", &some_test_type.string));
    ASSERT_TRUE(expected->GetInteger("integer", &some_test_type.integer));
    ASSERT_TRUE(expected->GetBoolean("boolean", &some_test_type.boolean));

    base::Value results = base::Value::FromUniquePtrValue(
        simple_api::OnTestTypeFired::Create(some_test_type));
    ASSERT_TRUE(results.is_list());
    ASSERT_EQ(1u, results.GetList().size());
    EXPECT_EQ(*expected, results.GetList()[0]);
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
      "key_enum": "one"
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
    "key_enum": "one"
  })";

  std::string error;
  ASSERT_NO_FATAL_FAILURE(GetManifestParseError(kPartialManifestJson, &error));
  EXPECT_EQ(
      "Error at key 'key_ref.array'. Parsing array failed at index 2: expected "
      "string, got integer",
      error);
}

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
    "key_enum": "one"
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
      "obj_bool": true
    },
    "key_enum": "one"
  })";

  simple_api::ManifestKeys manifest_keys;
  ASSERT_NO_FATAL_FAILURE(
      PopulateManifestKeys(kPartialManifestJson, &manifest_keys));

  EXPECT_EQ("abc", manifest_keys.key_string);

  ASSERT_TRUE(manifest_keys.key_obj);
  EXPECT_EQ("foo", manifest_keys.key_obj->obj_string);
  EXPECT_TRUE(manifest_keys.key_obj->obj_bool);

  EXPECT_EQ(simple_api::TEST_ENUM_ONE, manifest_keys.key_enum);

  EXPECT_EQ("ref_string", manifest_keys.key_ref.string);
  EXPECT_EQ(true, manifest_keys.key_ref.boolean);
  EXPECT_DOUBLE_EQ(25.4, manifest_keys.key_ref.number);
  EXPECT_EQ(32, manifest_keys.key_ref.integer);
  ASSERT_TRUE(manifest_keys.key_ref.object);
  EXPECT_EQ(42, manifest_keys.key_ref.object->foo);
  ASSERT_TRUE(manifest_keys.key_ref.array);
  EXPECT_THAT(*manifest_keys.key_ref.array,
              ::testing::ElementsAre("one", "two"));
  EXPECT_EQ(enums::ENUMERATION_TWO, manifest_keys.key_ref.opt_external_enum);
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
    "key_enum": "two"
  })";

  simple_api::ManifestKeys manifest_keys;
  ASSERT_NO_FATAL_FAILURE(
      PopulateManifestKeys(kPartialManifestJson, &manifest_keys));

  EXPECT_EQ("abc", manifest_keys.key_string);
  EXPECT_FALSE(manifest_keys.key_obj);
  EXPECT_EQ(simple_api::TEST_ENUM_TWO, manifest_keys.key_enum);

  EXPECT_EQ("ref_string", manifest_keys.key_ref.string);
  EXPECT_EQ(true, manifest_keys.key_ref.boolean);
  EXPECT_DOUBLE_EQ(25.4, manifest_keys.key_ref.number);
  EXPECT_EQ(32, manifest_keys.key_ref.integer);
  EXPECT_FALSE(manifest_keys.key_ref.array);
  EXPECT_EQ(enums::ENUMERATION_NONE, manifest_keys.key_ref.opt_external_enum);
}
