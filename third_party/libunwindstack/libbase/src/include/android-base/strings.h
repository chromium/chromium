/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <ctype.h>

#include <iterator>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace android {
namespace base {

// Splits a string into a vector of strings.
//
// The string is split at each occurrence of a character in delimiters.
//
// The empty string is not a valid delimiter list.
std::vector<std::string> Split(const std::string& s,
                               const std::string& delimiters);

// Splits a string into a vector of string tokens.
//
// The string is split at each occurrence of a character in delimiters.
// Coalesce runs of delimiter bytes and ignore delimiter bytes at the start or
// end of string. In other words, return only nonempty string tokens.
// Use when you don't care about recovering the original string with Join().
//
// Example:
//   Tokenize(" foo  bar ", " ") => {"foo", "bar"}
//   Join(Tokenize("  foo  bar", " "), " ") => "foo bar"
//
// The empty string is not a valid delimiter list.
std::vector<std::string> Tokenize(const std::string& s, const std::string& delimiters);

namespace internal {
template <typename>
constexpr bool always_false_v = false;
}

template <typename T>
[[nodiscard]] std::string Trim(T&& t) {
  std::string_view sv;
  std::string s;
  if constexpr (std::is_convertible_v<T, std::string_view>) {
    sv = std::forward<T>(t);
  } else if constexpr (std::is_convertible_v<T, std::string>) {
    // The previous version of this function allowed for types which are implicitly convertible
    // to std::string but not to std::string_view. For these types we go through std::string first
    // here in order to retain source compatibility.
    s = t;
    sv = s;
  } else {
    static_assert(internal::always_false_v<T>,
                  "Implicit conversion to std::string or std::string_view not possible");
  }

  // Skip initial whitespace.
  while (!sv.empty() && isspace(sv.front())) {
    sv.remove_prefix(1);
  }

  // Skip terminating whitespace.
  while (!sv.empty() && isspace(sv.back())) {
    sv.remove_suffix(1);
  }

  return std::string(sv);
}

// We instantiate the common cases in strings.cpp.
extern template std::string Trim(const char*&);
extern template std::string Trim(const char*&&);
extern template std::string Trim(const std::string&);
extern template std::string Trim(const std::string&&);
extern template std::string Trim(std::string_view&);
extern template std::string Trim(std::string_view&&);

// Joins a container of things into a single string, using the given separator.
template <typename ContainerT, typename SeparatorT>
std::string Join(ContainerT&& things, SeparatorT separator) {
  using ElementType = typename std::remove_reference_t<ContainerT>::value_type;

  if (things.empty()) {
    return {};
  } else if (things.size() == 1) {
    // Nothing to do! Return the first element if it's already a string-like type, otherwise
    // fallthrough to the slower format-conversion case at the bottom of this function.

    if constexpr (std::is_convertible_v<ElementType, std::string>) {
      return *things.begin();
    } else if constexpr (std::is_constructible_v<std::string, ElementType>) {
      // std::string_view is not implicitly convertible to std::string so do it explicitly, making
      // a copy in this case.
      return std::string(*things.begin());
    }
  }

  if constexpr (std::is_convertible_v<ElementType, std::string_view>) {
    // String-like types are what the vast majority of callers use.
    // Use a much faster implementation for these types.

    // char separator types need special handling because they cannot be converted to
    // std::string_view to determine their size, and they require a special std::string::append
    // invocation below.
    constexpr bool sepIsChar = std::is_same_v<std::remove_cv_t<SeparatorT>, char>;
    std::string_view::size_type sepSize;
    if constexpr (sepIsChar) sepSize = 1;
    else                     sepSize = std::string_view(separator).size();

    const std::string_view::size_type total = std::accumulate(
        std::next(things.begin()), things.end(), std::string_view(*things.begin()).size(),
        [&sepSize](std::string_view::size_type sum, std::string_view sv) {
          return sum + sepSize + sv.size();
        }
    );

    std::string result;
    result.reserve(total);  // allocate once
    result.append(*things.begin());
    for(auto it = std::next(things.begin()); it != things.end(); ++it) {
      if constexpr (sepIsChar) result.append(1, separator).append(*it);
      else                     result.append(separator).append(*it);
    }
    return result;

  } else {
    // Some callers depend on the conversion performed by std::ostream:operator<< to get string
    // representations from non-string types.

    std::ostringstream result;
    result << *things.begin();
    for (auto it = std::next(things.begin()); it != things.end(); ++it) {
      result << separator << *it;
    }
    return result.str();
  }
}

// These cases were measured either to be used during build by more than one binary, or during
// runtime as a significant portion of total calls.
// Instantiate them in strings.cpp to aid compile time and binary size.
extern template std::string Join(std::vector<std::string>&, char);
extern template std::string Join(std::vector<std::string>&, const char*);
extern template std::string Join(std::vector<std::string>&&, const char*);
extern template std::string Join(const std::vector<std::string>&, char);
extern template std::string Join(const std::vector<std::string>&, const char*);
extern template std::string Join(const std::vector<std::string>&&, const char*);
extern template std::string Join(std::set<std::string>&, const char*);
extern template std::string Join(const std::set<std::string>&, char);
extern template std::string Join(const std::set<std::string>&, const char*);
extern template std::string Join(const std::unordered_set<std::string>&, const char*);

// Tests whether 's' starts with 'prefix'.
bool StartsWith(std::string_view s, std::string_view prefix);
bool StartsWith(std::string_view s, char prefix);
bool StartsWithIgnoreCase(std::string_view s, std::string_view prefix);

// Tests whether 's' ends with 'suffix'.
bool EndsWith(std::string_view s, std::string_view suffix);
bool EndsWith(std::string_view s, char suffix);
bool EndsWithIgnoreCase(std::string_view s, std::string_view suffix);

// Tests whether 'lhs' equals 'rhs', ignoring case.
bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs);

// Removes `prefix` from the start of the given string and returns true (if
// it was present), false otherwise.
inline bool ConsumePrefix(std::string_view* s, std::string_view prefix) {
  if (!StartsWith(*s, prefix)) return false;
  s->remove_prefix(prefix.size());
  return true;
}

// Removes `suffix` from the end of the given string and returns true (if
// it was present), false otherwise.
inline bool ConsumeSuffix(std::string_view* s, std::string_view suffix) {
  if (!EndsWith(*s, suffix)) return false;
  s->remove_suffix(suffix.size());
  return true;
}

// Replaces `from` with `to` in `s`, once if `all == false`, or as many times as
// there are matches if `all == true`.
[[nodiscard]] std::string StringReplace(std::string_view s, std::string_view from,
                                        std::string_view to, bool all);

// Converts an errno number to its error message string.
std::string ErrnoNumberAsString(int errnum);

}  // namespace base
}  // namespace android
