// Copyright 2020 The Chromium Authors. All rights reserved.
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/utils.h"

namespace liburlpattern {

namespace {

constexpr absl::string_view kRegexpSpecialCharacters(".+*?^${}()[]|/\\");
constexpr absl::string_view kPatternSpecialCharacters("+*?:{}()\\");

void EscapeStringAndAppendInternal(absl::string_view input,
                                   std::string& append_target,
                                   absl::string_view special_chars) {
  for (auto& c : input) {
    if (special_chars.find(c) != std::string::npos)
      append_target += '\\';
    append_target += c;
  }
}

}  // namespace

size_t EscapedRegexpStringLength(absl::string_view input) {
  size_t count = input.size();
  for (auto& c : input) {
    if (kRegexpSpecialCharacters.find(c) != std::string::npos)
      count += 1;
  }
  return count;
}

void EscapeRegexpStringAndAppend(absl::string_view input,
                                 std::string& append_target) {
  return EscapeStringAndAppendInternal(input, append_target,
                                       kRegexpSpecialCharacters);
}

void EscapePatternStringAndAppend(absl::string_view input,
                                  std::string& append_target) {
  return EscapeStringAndAppendInternal(input, append_target,
                                       kPatternSpecialCharacters);
}

std::string EscapeRegexpString(absl::string_view input) {
  std::string result;
  result.reserve(EscapedRegexpStringLength(input));
  EscapeRegexpStringAndAppend(input, result);
  return result;
}

}  // namespace liburlpattern
