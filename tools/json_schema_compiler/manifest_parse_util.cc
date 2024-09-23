// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/manifest_parse_util.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace json_schema_compiler {
namespace manifest_parse_util {

namespace {

// Alias for a pointer to a base::Value const function which converts the
// base::Value into type T. This is used by ParseHelper below.
template <typename T>
using ValueTypeConverter = T (base::Value::*)() const;

template <typename T, typename U>
bool ParseHelper(const base::Value::Dict& dict,
                 std::string_view key,
                 base::Value::Type expected_type,
                 ValueTypeConverter<U> type_converter,
                 T& out,
                 std::u16string& error,
                 std::vector<std::string_view>& error_path_reversed) {
  DCHECK(type_converter);

  const base::Value* value =
      FindKeyOfType(dict, key, expected_type, error, error_path_reversed);
  if (!value)
    return false;

  out = (value->*type_converter)();
  return true;
}

}  // namespace

void PopulateInvalidEnumValueError(
    std::string_view key,
    std::string_view value,
    std::u16string& error,
    std::vector<std::string_view>& error_path_reversed) {
  DCHECK(error.empty());
  DCHECK(error_path_reversed.empty());

  error_path_reversed.push_back(key);
  error = base::ASCIIToUTF16(base::StringPrintf(
      "Specified value '%s' is invalid.", std::string(value).c_str()));
}

void PopulateInvalidChoiceValueError(
    std::string_view key,
    std::u16string& error,
    std::vector<std::string_view>& error_path_reversed) {
  DCHECK(error.empty());
  DCHECK(error_path_reversed.empty());

  error_path_reversed.push_back(key);
  error = base::ASCIIToUTF16(base::StringPrintf(
      "Provided value matches none of the allowed options."));
}

void PopulateKeyIsRequiredError(
    std::string_view key,
    std::u16string& error,
    std::vector<std::string_view>& error_path_reversed) {
  DCHECK(error.empty());
  DCHECK(error_path_reversed.empty());

  error_path_reversed.push_back(key);
  error = u"Manifest key is required.";
}

std::u16string GetArrayParseError(size_t error_index,
                                  const std::u16string& item_error) {
  return base::ASCIIToUTF16(
      base::StringPrintf("Parsing array failed at index %" PRIuS ": %s",
                         error_index, base::UTF16ToASCII(item_error).c_str()));
}

void PopulateFinalError(std::u16string& error,
                        std::vector<std::string_view>& error_path_reversed) {
  DCHECK(!error.empty());
  DCHECK(!error_path_reversed.empty());

  // Reverse the path to ensure the constituent keys are in the correct order.
  std::reverse(error_path_reversed.begin(), error_path_reversed.end());
  error = base::ASCIIToUTF16(
      base::StringPrintf("Error at key '%s'. %s",
                         base::JoinString(error_path_reversed, ".").c_str(),
                         base::UTF16ToASCII(error).c_str()));
}

const base::Value* FindKeyOfType(
    const base::Value::Dict& dict,
    std::string_view key,
    base::Value::Type expected_type,
    std::u16string& error,
    std::vector<std::string_view>& error_path_reversed) {
  DCHECK(error.empty());
  DCHECK(error_path_reversed.empty());

  const base::Value* value = dict.Find(key);
  if (!value) {
    PopulateKeyIsRequiredError(key, error, error_path_reversed);
    return nullptr;
  }

  if (value->type() != expected_type) {
    error_path_reversed.push_back(key);
    error = base::ASCIIToUTF16(
        base::StringPrintf("Type is invalid. Expected %s, found %s.",
                           base::Value::GetTypeName(expected_type),
                           base::Value::GetTypeName(value->type())));
    return nullptr;
  }

  return value;
}

bool ParseFromDictionary(const base::Value::Dict& dict,
                         std::string_view key,
                         int& out,
                         std::u16string& error,
                         std::vector<std::string_view>& error_path_reversed) {
  return ParseHelper(dict, key, base::Value::Type::INTEGER,
                     &base::Value::GetInt, out, error, error_path_reversed);
}

bool ParseFromDictionary(const base::Value::Dict& dict,
                         std::string_view key,
                         bool& out,
                         std::u16string& error,
                         std::vector<std::string_view>& error_path_reversed) {
  return ParseHelper(dict, key, base::Value::Type::BOOLEAN,
                     &base::Value::GetBool, out, error, error_path_reversed);
}

bool ParseFromDictionary(const base::Value::Dict& dict,
                         std::string_view key,
                         double& out,
                         std::u16string& error,
                         std::vector<std::string_view>& error_path_reversed) {
  return ParseHelper(dict, key, base::Value::Type::DOUBLE,
                     &base::Value::GetDouble, out, error, error_path_reversed);
}

bool ParseFromDictionary(const base::Value::Dict& dict,
                         std::string_view key,
                         std::string& out,
                         std::u16string& error,
                         std::vector<std::string_view>& error_path_reversed) {
  return ParseHelper(dict, key, base::Value::Type::STRING,
                     &base::Value::GetString, out, error, error_path_reversed);
}

}  // namespace manifest_parse_util
}  // namespace json_schema_compiler
