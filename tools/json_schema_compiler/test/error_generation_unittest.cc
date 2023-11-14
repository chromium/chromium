// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/error_generation.h"

#include <memory>
#include <vector>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/test_util.h"

namespace errors = test::api::error_generation;
using base::Value;
using json_schema_compiler::test_util::Dictionary;
using json_schema_compiler::test_util::List;

template <typename T, typename Value>
std::u16string GetPopulateError(const Value& value) {
  return T::FromValue(value).error_or(std::u16string());
}

testing::AssertionResult EqualsUtf16(const std::string& expected,
                                     const std::u16string& actual) {
  if (base::ASCIIToUTF16(expected) == actual)
    return testing::AssertionSuccess();
  return testing::AssertionFailure() << "\n    actual:     " << actual
                                     << "\n    expected:   " << expected;
}

// GenerateTypePopulate errors

TEST(JsonSchemaCompilerErrorTest, RequiredPropertyPopulate) {
  {
    base::Value value = Dictionary("string", Value("bling"));
    EXPECT_TRUE(
        EqualsUtf16("", GetPopulateError<errors::TestType>(value.GetDict())));
  }
}

TEST(JsonSchemaCompilerErrorTest, UnexpectedTypePopulation) {
  {
    base::Value value(Value::Type::LIST);
    EXPECT_TRUE(
        EqualsUtf16("", GetPopulateError<errors::ChoiceType::Integers>(value)));
  }
  {
    base::Value value(Value::Type::BINARY);
    EXPECT_TRUE(
        EqualsUtf16("expected integers or integer, got binary",
                    GetPopulateError<errors::ChoiceType::Integers>(value)));
  }
}

// GenerateTypePopulateProperty errors

TEST(JsonSchemaCompilerErrorTest, TypeIsRequired) {
  {
    base::Value value = Dictionary("integers", Value(5));
    EXPECT_TRUE(
        EqualsUtf16("", GetPopulateError<errors::ChoiceType>(value.GetDict())));
  }
  {
    base::Value::Dict value;
    EXPECT_TRUE(EqualsUtf16("'integers' is required",
                            GetPopulateError<errors::ChoiceType>(value)));
  }
}

// GenerateParamsCheck errors

TEST(JsonSchemaCompilerErrorTest, TooManyParameters) {
  {
    base::Value::List params_value;
    params_value.Append(5);
    EXPECT_TRUE(errors::TestFunction::Params::Create(params_value).has_value());
  }
  {
    base::Value::List params_value;
    params_value.Append(5);
    params_value.Append(5);
    EXPECT_TRUE(EqualsUtf16("expected 1 arguments, got 2",
                            errors::TestFunction::Params::Create(params_value)
                                .error_or(std::u16string())));
  }
}

// GenerateFunctionParamsCreate errors

TEST(JsonSchemaCompilerErrorTest, ParamIsRequired) {
  {
    base::Value::List params_value;
    params_value.Append(5);
    EXPECT_TRUE(errors::TestFunction::Params::Create(params_value).has_value());
  }
  {
    base::Value::List params_value;
    params_value.Append(base::Value());
    EXPECT_TRUE(EqualsUtf16("'num' is required",
                            errors::TestFunction::Params::Create(params_value)
                                .error_or(std::u16string())));
  }
}

// GeneratePopulateVariableFromValue errors

TEST(JsonSchemaCompilerErrorTest, WrongPropertyValueType) {
  {
    base::Value value = Dictionary("string", Value("yes"));
    EXPECT_TRUE(
        EqualsUtf16("", GetPopulateError<errors::TestType>(value.GetDict())));
  }
  {
    base::Value value = Dictionary("string", Value(1.1));
    EXPECT_TRUE(
        EqualsUtf16("'string': expected string, got double",
                    GetPopulateError<errors::TestType>(value.GetDict())));
  }
}

TEST(JsonSchemaCompilerErrorTest, WrongParameterCreationType) {
  {
    std::u16string error;
    base::Value::List params_value;
    params_value.Append("Yeah!");
    EXPECT_TRUE(errors::TestString::Params::Create(params_value).has_value());
  }
  {
    base::Value::List params_value;
    params_value.Append(5);
    std::u16string error;
    EXPECT_TRUE(
        EqualsUtf16("'paramObject': expected dictionary, got integer",
                    errors::TestTypeInObject::Params::Create(params_value)
                        .error_or(std::u16string())));
  }
}

TEST(JsonSchemaCompilerErrorTest, WrongTypeValueType) {
  {
    base::Value::Dict value;
    EXPECT_TRUE(EqualsUtf16("", GetPopulateError<errors::ObjectType>(value)));
  }
  {
    base::Value value = Dictionary("otherType", Value(1.1));
    auto out = errors::ObjectType::FromValue(value.GetDict());
    ASSERT_FALSE(out.has_value());
    EXPECT_TRUE(EqualsUtf16("'otherType': expected dictionary, got double",
                            out.error()));
  }
}

TEST(JsonSchemaCompilerErrorTest, UnableToPopulateArray) {
  {
    base::Value params_value = List(Value(5));
    EXPECT_TRUE(EqualsUtf16(
        "", GetPopulateError<errors::ChoiceType::Integers>(params_value)));
  }
  {
    base::Value params_value = List(Value(5), Value(false));
    EXPECT_TRUE(EqualsUtf16(
        "Error at key 'integers': Parsing array failed at index 1: expected "
        "integer, got boolean",
        GetPopulateError<errors::ChoiceType::Integers>(params_value)));
  }
}

TEST(JsonSchemaCompilerErrorTest, BinaryTypeExpected) {
  {
    base::Value value = Dictionary("data", Value(Value::Type::BINARY));
    EXPECT_TRUE(
        EqualsUtf16("", GetPopulateError<errors::BinaryData>(value.GetDict())));
  }
  {
    base::Value value = Dictionary("data", Value(1.1));
    EXPECT_TRUE(
        EqualsUtf16("'data': expected binary, got double",
                    GetPopulateError<errors::BinaryData>(value.GetDict())));
  }
}

TEST(JsonSchemaCompilerErrorTest, ListExpected) {
  {
    base::Value value =
        Dictionary("TheArray", base::Value(base::Value::Type::LIST));
    EXPECT_TRUE(EqualsUtf16(
        "", GetPopulateError<errors::ArrayObject>(value.GetDict())));
  }
  {
    base::Value value = Dictionary("TheArray", Value(5));
    EXPECT_TRUE(
        EqualsUtf16("'TheArray': expected list, got integer",
                    GetPopulateError<errors::ArrayObject>(value.GetDict())));
  }
}

// GenerateStringToEnumConversion errors

TEST(JsonSchemaCompilerErrorTest, BadEnumValue) {
  {
    base::Value value = Dictionary("enumeration", Value("one"));
    EXPECT_TRUE(EqualsUtf16(
        "", GetPopulateError<errors::HasEnumeration>(value.GetDict())));
  }
  {
    base::Value value = Dictionary("enumeration", Value("bad sauce"));
    EXPECT_TRUE(
        EqualsUtf16("'Enumeration': expected \"one\" or \"two\" "
                    "or \"three\", got \"bad sauce\"",
                    GetPopulateError<errors::HasEnumeration>(value.GetDict())));
  }
}

TEST(JsonSchemaCompilerErrorTest, ErrorOnOptionalFailure) {
  {
    base::Value value = Dictionary("string", Value("bling"));
    EXPECT_TRUE(EqualsUtf16(
        "", GetPopulateError<errors::OptionalTestType>(value.GetDict())));
  }
  {
    base::Value value = Dictionary("string", Value(1));

    auto out = errors::OptionalTestType::FromValue(value.GetDict());
    ASSERT_FALSE(out.has_value());
    EXPECT_TRUE(
        EqualsUtf16("'string': expected string, got integer", out.error()));
  }
}

TEST(JsonSchemaCompilerErrorTest, OptionalBinaryTypeFailure) {
  {
    base::Value value = Dictionary("data", Value(Value::Type::BINARY));
    EXPECT_TRUE(EqualsUtf16(
        "", GetPopulateError<errors::OptionalBinaryData>(value.GetDict())));
  }
  {
    // There's a bug with silent failures if the key doesn't exist.
    base::Value value = Dictionary("data", Value(1));

    auto out = errors::OptionalBinaryData::FromValue(value.GetDict());
    ASSERT_FALSE(out.has_value());
    EXPECT_TRUE(
        EqualsUtf16("'data': expected binary, got integer", out.error()));
  }
}

TEST(JsonSchemaCompilerErrorTest, OptionalArrayTypeFailure) {
  {
    base::Value value =
        Dictionary("TheArray", base::Value(base::Value::Type::LIST));
    EXPECT_TRUE(EqualsUtf16(
        "", GetPopulateError<errors::ArrayObject>(value.GetDict())));
  }
  {
    base::Value value = Dictionary("TheArray", Value(5));
    auto out = errors::ArrayObject::FromValue(value.GetDict());
    EXPECT_FALSE(out.has_value());
    EXPECT_TRUE(
        EqualsUtf16("'TheArray': expected list, got integer", out.error()));
  }
}

TEST(JsonSchemaCompilerErrorTest, OptionalUnableToPopulateArray) {
  {
    base::Value params_value = List(Value(5));
    EXPECT_TRUE(EqualsUtf16(
        "",
        GetPopulateError<errors::OptionalChoiceType::Integers>(params_value)));
  }
  {
    base::Value params_value = List(Value(5), Value(false));
    auto out = errors::OptionalChoiceType::Integers::FromValue(params_value);
    EXPECT_FALSE(out.has_value());
    EXPECT_TRUE(
        EqualsUtf16("Error at key 'integers': Parsing array failed at index 1: "
                    "expected integer, got boolean",
                    out.error()));
  }
}

TEST(JsonSchemaCompilerErrorTest, TooManyKeys) {
  {
    base::Value value = Dictionary("string", Value("yes"));
    EXPECT_TRUE(
        EqualsUtf16("", GetPopulateError<errors::TestType>(value.GetDict())));
  }
  {
    // We simply ignore extra keys.
    base::Value value =
        Dictionary("string", Value("yes"), "ohno", Value("many values"));
    EXPECT_TRUE(
        EqualsUtf16("", GetPopulateError<errors::TestType>(value.GetDict())));
  }
}
