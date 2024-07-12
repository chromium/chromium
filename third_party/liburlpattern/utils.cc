// Copyright 2020 The Chromium Authors
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/utils.h"

#include <string_view>

namespace liburlpattern {

namespace {

constexpr std::string_view kRegexpSpecialCharacters(".+*?^${}()[]|/\\");
constexpr std::string_view kPatternSpecialCharacters("+*?:{}()\\");

void EscapeStringAndAppendInternal(std::string_view input,
                                   std::string& append_target,
                                   std::string_view special_chars) {
  for (auto& c : input) {
    if (special_chars.find(c) != std::string::npos)
      append_target += '\\';
    append_target += c;
  }
}

}  // namespace

size_t EscapedRegexpStringLength(std::string_view input) {
  size_t count = input.size();
  for (auto& c : input) {
    if (kRegexpSpecialCharacters.find(c) != std::string::npos)
      count += 1;
  }
  return count;
}

void EscapeRegexpStringAndAppend(std::string_view input,
                                 std::string& append_target) {
  return EscapeStringAndAppendInternal(input, append_target,
                                       kRegexpSpecialCharacters);
}

void EscapePatternStringAndAppend(std::string_view input,
                                  std::string& append_target) {
  return EscapeStringAndAppendInternal(input, append_target,
                                       kPatternSpecialCharacters);
}

std::string EscapeRegexpString(std::string_view input) {
  std::string result;
  result.reserve(EscapedRegexpStringLength(input));
  EscapeRegexpStringAndAppend(input, result);
  return result;
}

bool IsNameCodepoint(UChar32 c, bool first_codepoint) {
  // Require group names to follow the same character restrictions as
  // javascript identifiers.  This code originates from v8 at:
  //
  // https://source.chromium.org/chromium/chromium/src/+/master:v8/src/strings/char-predicates.cc;l=17-34;drc=be014256adea1552d4a044ef80616cdab6a7d549
  //
  // We deviate from js identifiers, however, in not support the backslash
  // character.  This is mainly used in js identifiers to allow escaped
  // unicode sequences to be written in ascii.  The js engine, however,
  // should take care of this long before we reach this level of code.  So
  // we don't need to handle it here.
  if (first_codepoint) {
    return u_hasBinaryProperty(c, UCHAR_ID_START) || c == '$' || c == '_';
  }
  return u_hasBinaryProperty(c, UCHAR_ID_CONTINUE) || c == '$' || c == '_' ||
         c == 0x200c || c == 0x200d;
}

}  // namespace liburlpattern
