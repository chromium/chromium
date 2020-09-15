// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_JSON_SCHEMA_COMPILER_MANIFEST_PARSE_UTIL_H_
#define TOOLS_JSON_SCHEMA_COMPILER_MANIFEST_PARSE_UTIL_H_

#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "tools/json_schema_compiler/util.h"

namespace json_schema_compiler {
namespace manifest_parse_util {

// This file contains helpers used by auto-generated manifest parsing code.

// Populates |error| and |error_path_reversed| denoting array parse error at the
// given |key|. Note |error| should already contain the specific parse error for
// the array item.
void PopulateArrayParseError(
    base::StringPiece key,
    base::string16* error,
    std::vector<base::StringPiece>* error_path_reversed);

// Populates |error| and |error_path_reversed| denoting the given invalid enum
// |value| at the given |key|.
void PopulateInvalidEnumValueError(
    base::StringPiece key,
    const std::string& value,
    base::string16* error,
    std::vector<base::StringPiece>* error_path_reversed);

// Populates manifest parse |error| for the given path in |error_path_reversed|.
void PopulateFinalError(base::string16* error,
                        std::vector<base::StringPiece>* error_path_reversed);

// Returns the value at the given |key| in |dict|, ensuring that it's of the
// |expected_type|. On failure, returns false and populates |error| and
// |error_path_reversed|.
const base::Value* FindKeyOfType(
    const base::DictionaryValue& dict,
    base::StringPiece key,
    base::Value::Type expected_type,
    base::string16* error,
    std::vector<base::StringPiece>* error_path_reversed);

// Parses |out| from |dict| at the given |key|. On failure, returns false and
// populates |error| and |error_path_reversed|.
bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         int* out,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed);
bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         bool* out,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed);
bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         double* out,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed);
bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         std::string* out,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed);

// This overload is used for lists/arrays.
template <typename T>
bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         std::vector<T>* out_ptr,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed);

// This overload is used for optional values.
template <typename T>
bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         std::unique_ptr<T>* out_ptr,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed);

// This overload is used for generated types.
template <typename T>
bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         T* out_ptr,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed) {
  return T::ParseFromDictionary(dict, key, out_ptr, error, error_path_reversed);
}

template <typename T>
bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         std::vector<T>* out_ptr,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed) {
  const base::Value* value = FindKeyOfType(dict, key, base::Value::Type::LIST,
                                           error, error_path_reversed);
  if (!value)
    return false;

  bool result = json_schema_compiler::util::PopulateArrayFromList(
      value->AsListValue(*value), out_ptr, error);
  if (!result)
    PopulateArrayParseError(key, error, error_path_reversed);

  return result;
}

template <typename T>
bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         std::unique_ptr<T>* out_ptr,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed) {
  DCHECK(out_ptr);

  // Ignore optional keys if they are not present without raising an error.
  if (!dict.FindKey(key))
    return true;

  // Parse errors for optional keys which are specified should still cause a
  // failure.
  auto result = std::make_unique<T>();
  if (!ParseFromDictionary(dict, key, result.get(), error, error_path_reversed))
    return false;

  *out_ptr = std::move(result);
  return true;
}

// Alias for pointer to a function which converts a string to an enum of type T.
template <typename T>
using StringToEnumConverter = T (*)(const std::string&);

// Parses enum |out| from |dict| at the given |key|. On failure, returns false
// and populates |error| and |error_path_reversed|.
template <typename T>
bool ParseEnumFromDictionary(
    const base::DictionaryValue& dict,
    base::StringPiece key,
    StringToEnumConverter<T> converter,
    bool is_optional_property,
    T none_value,
    T* out,
    base::string16* error,
    std::vector<base::StringPiece>* error_path_reversed) {
  DCHECK(out);
  DCHECK_EQ(none_value, *out);

  // Ignore optional keys if they are not present without raising an error.
  if (is_optional_property && !dict.FindKey(key))
    return true;

  // Parse errors for optional keys which are specified should still cause a
  // failure.
  const base::Value* value = FindKeyOfType(dict, key, base::Value::Type::STRING,
                                           error, error_path_reversed);
  if (!value)
    return false;

  const std::string str = value->GetString();
  T enum_value = converter(str);
  if (enum_value == none_value) {
    PopulateInvalidEnumValueError(key, str, error, error_path_reversed);
    return false;
  }

  *out = enum_value;
  return true;
}

}  // namespace manifest_parse_util
}  // namespace json_schema_compiler

#endif  // TOOLS_JSON_SCHEMA_COMPILER_MANIFEST_PARSE_UTIL_H_
