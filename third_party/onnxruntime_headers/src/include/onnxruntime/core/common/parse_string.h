// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <locale>
#include <sstream>
#include <string_view>
#include <type_traits>

#include "core/common/common.h"

namespace onnxruntime {

/**
 * Tries to parse a value from an entire string.
 */
template <typename T>
bool TryParseStringWithClassicLocale(std::string_view str, T& value) {
  if constexpr (std::is_integral<T>::value && std::is_unsigned<T>::value) {
    // if T is unsigned integral type, reject negative values which will wrap
    if (!str.empty() && str[0] == '-') {
      return false;
    }
  }

  // don't allow leading whitespace
  if (!str.empty() && std::isspace(str[0], std::locale::classic())) {
    return false;
  }

  std::istringstream is{std::string{str}};
  is.imbue(std::locale::classic());
  T parsed_value{};

  const bool parse_successful =
      is >> parsed_value &&
      is.get() == std::istringstream::traits_type::eof();  // don't allow trailing characters
  if (!parse_successful) {
    return false;
  }

  value = std::move(parsed_value);
  return true;
}

inline bool TryParseStringWithClassicLocale(std::string_view str, std::string& value) {
  value = str;
  return true;
}

inline bool TryParseStringWithClassicLocale(std::string_view str, bool& value) {
  if (str == "0" || str == "False" || str == "false") {
    value = false;
    return true;
  }

  if (str == "1" || str == "True" || str == "true") {
    value = true;
    return true;
  }

  return false;
}

/**
 * Parses a value from an entire string.
 */
template <typename T>
Status ParseStringWithClassicLocale(std::string_view s, T& value) {
  ORT_RETURN_IF_NOT(TryParseStringWithClassicLocale(s, value), "Failed to parse value: \"", value, "\"");
  return Status::OK();
}

/**
 * Parses a value from an entire string.
 */
template <typename T>
T ParseStringWithClassicLocale(std::string_view s) {
  T value{};
  ORT_THROW_IF_ERROR(ParseStringWithClassicLocale(s, value));
  return value;
}

}  // namespace onnxruntime
