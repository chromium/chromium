// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_JSON_SCHEMA_COMPILER_MANIFEST_PARSE_UTIL_H_
#define TOOLS_JSON_SCHEMA_COMPILER_MANIFEST_PARSE_UTIL_H_

#include <string>
#include <vector>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "tools/json_schema_compiler/util.h"

namespace json_schema_compiler {
namespace manifest_parse_util {

// This file contains helpers used by auto-generated manifest parsing code.

// Populates |error| and |error_path_reversed| denoting the given invalid enum
// |value| at the given |key|.
void PopulateInvalidEnumValueError(
    base::StringPiece key,
    base::StringPiece value,
    std::u16string& error,
    std::vector<base::StringPiece>& error_path_reversed);

// Returns array parse error for `item_error` at index `error_index`
std::u16string GetArrayParseError(size_t error_index,
                                  const std::u16string& item_error);

// Populates manifest parse |error| for the given path in |error_path_reversed|.
void PopulateFinalError(std::u16string& error,
                        std::vector<base::StringPiece>& error_path_reversed);

// Returns the value at the given |key| in |dict|, ensuring that it's of the
// |expected_type|. On failure, returns false and populates |error| and
// |error_path_reversed|.
const base::Value* FindKeyOfType(
    const base::Value::Dict& dict,
    base::StringPiece key,
    base::Value::Type expected_type,
    std::u16string& error,
    std::vector<base::StringPiece>& error_path_reversed);

// Parses |out| from |dict| at the given |key|. On failure, returns false and
// populates |error| and |error_path_reversed|.
bool ParseFromDictionary(const base::Value::Dict& dict,
                         base::StringPiece key,
                         int& out,
                         std::u16string& error,
                         std::vector<base::StringPiece>& error_path_reversed);
bool ParseFromDictionary(const base::Value::Dict& dict,
                         base::StringPiece key,
                         bool& out,
                         std::u16string& error,
                         std::vector<base::StringPiece>& error_path_reversed);
bool ParseFromDictionary(const base::Value::Dict& dict,
                         base::StringPiece key,
                         double& out,
                         std::u16string& error,
                         std::vector<base::StringPiece>& error_path_reversed);
bool ParseFromDictionary(const base::Value::Dict& dict,
                         base::StringPiece key,
                         std::string& out,
                         std::u16string& error,
                         std::vector<base::StringPiece>& error_path_reversed);

// This overload is used for lists/arrays.
template <typename T>
bool ParseFromDictionary(const base::Value::Dict& dict,
                         base::StringPiece key,
                         std::vector<T>& out,
                         std::u16string& error,
                         std::vector<base::StringPiece>& error_path_reversed);

// This overload is used for optional types wrapped as unique_ptr<T>.
template <typename T>
bool ParseFromDictionary(const base::Value::Dict& dict,
                         base::StringPiece key,
                         std::unique_ptr<T>& out,
                         std::u16string& error,
                         std::vector<base::StringPiece>& error_path_reversed);

// This overload is used for optional types wrapped as absl::optional<T>.
template <typename T>
bool ParseFromDictionary(const base::Value::Dict& dict,
                         base::StringPiece key,
                         absl::optional<T>& out_opt,
                         std::u16string& error,
                         std::vector<base::StringPiece>& error_path_reversed);

// This overload is used for generated types.
template <typename T>
bool ParseFromDictionary(const base::Value::Dict& dict,
                         base::StringPiece key,
                         T& out,
                         std::u16string& error,
                         std::vector<base::StringPiece>& error_path_reversed) {
  return T::ParseFromDictionary(dict, key, out, error, error_path_reversed);
}

template <typename T>
bool ParseFromDictionary(const base::Value::Dict& dict,
                         base::StringPiece key,
                         std::vector<T>& out,
                         std::u16string& error,
                         std::vector<base::StringPiece>& error_path_reversed) {
  const base::Value* value = FindKeyOfType(dict, key, base::Value::Type::LIST,
                                           error, error_path_reversed);
  if (!value)
    return false;

  bool result = json_schema_compiler::util::PopulateArrayFromList(
      value->GetList(), out, error);
  if (!result) {
    DCHECK(error_path_reversed.empty());
    error_path_reversed.push_back(key);
  }

  return result;
}

template <typename T>
bool ParseFromDictionary(const base::Value::Dict& dict,
                         base::StringPiece key,
                         std::unique_ptr<T>& out,
                         std::u16string& error,
                         std::vector<base::StringPiece>& error_path_reversed) {
  // Ignore optional keys if they are not present without raising an error.
  if (!dict.Find(key))
    return true;

  // Parse errors for optional keys which are specified should still cause a
  // failure.
  auto result = std::make_unique<T>();
  if (!ParseFromDictionary(dict, key, *result, error, error_path_reversed)) {
    return false;
  }

  out = std::move(result);
  return true;
}

template <typename T>
bool ParseFromDictionary(const base::Value::Dict& dict,
                         base::StringPiece key,
                         absl::optional<T>& out_opt,
                         std::u16string& error,
                         std::vector<base::StringPiece>& error_path_reversed) {
  // Ignore optional keys if they are not present without raising an error.
  if (!dict.Find(key))
    return true;

  // Parse errors for optional keys which are specified should still cause a
  // failure.
  T result{};
  if (!ParseFromDictionary(dict, key, result, error, error_path_reversed)) {
    return false;
  }

  out_opt = std::move(result);
  return true;
}

// Alias for pointer to a function which converts a string to an enum of type T.
template <typename T>
using StringToEnumConverter = T (*)(base::StringPiece);

// Parses enum |out| from |dict| at the given |key|. On failure, returns false
// and populates |error| and |error_path_reversed|.
template <typename T>
bool ParseEnumFromDictionary(
    const base::Value::Dict& dict,
    base::StringPiece key,
    StringToEnumConverter<T> converter,
    bool is_optional_property,
    T none_value,
    T& out,
    std::u16string& error,
    std::vector<base::StringPiece>& error_path_reversed) {
  DCHECK_EQ(none_value, out);

  // Ignore optional keys if they are not present without raising an error.
  if (is_optional_property && !dict.Find(key))
    return true;

  // Parse errors for optional keys which are specified should still cause a
  // failure.
  const base::Value* value = FindKeyOfType(dict, key, base::Value::Type::STRING,
                                           error, error_path_reversed);
  if (!value)
    return false;

  const std::string& str = value->GetString();
  T enum_value = converter(str);
  if (enum_value == none_value) {
    PopulateInvalidEnumValueError(key, str, error, error_path_reversed);
    return false;
  }

  out = enum_value;
  return true;
}

// Parses non-optional enum array `out` from `dict` at the given `key`. On
// failure, returns false and populates `error` and `error_path_reversed`.
template <typename T>
bool ParseEnumArrayFromDictionary(
    const base::Value::Dict& dict,
    base::StringPiece key,
    StringToEnumConverter<T> converter,
    T none_value,
    std::vector<T>& out,
    std::u16string& error,
    std::vector<base::StringPiece>& error_path_reversed) {
  std::vector<std::string> str_array;
  if (!ParseFromDictionary(dict, key, str_array, error, error_path_reversed)) {
    return false;
  }

  std::vector<T> result;
  result.reserve(str_array.size());
  for (size_t i = 0; i < str_array.size(); ++i) {
    T enum_value = converter(str_array[i]);
    if (enum_value == none_value) {
      std::u16string item_error;
      PopulateInvalidEnumValueError(key, str_array[i], item_error,
                                    error_path_reversed);
      error = GetArrayParseError(i, item_error);
      return false;
    }

    result.push_back(enum_value);
  }

  out = std::move(result);
  return true;
}

// Overload for optional enum arrays.
template <typename T>
bool ParseEnumArrayFromDictionary(
    const base::Value::Dict& dict,
    base::StringPiece key,
    StringToEnumConverter<T> converter,
    T none_value,
    absl::optional<std::vector<T>>& out,
    std::u16string& error,
    std::vector<base::StringPiece>& error_path_reversed) {
  // Ignore optional keys if they are not present without raising an error.
  if (!dict.Find(key))
    return true;

  // Parse errors for optional keys which are specified should still cause a
  // failure.
  std::vector<T> result;
  if (!ParseEnumArrayFromDictionary(dict, key, converter, none_value, result,
                                    error, error_path_reversed)) {
    return false;
  }

  out = std::move(result);
  return true;
}

}  // namespace manifest_parse_util
}  // namespace json_schema_compiler

#endif  // TOOLS_JSON_SCHEMA_COMPILER_MANIFEST_PARSE_UTIL_H_
