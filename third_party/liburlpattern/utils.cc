// Copyright 2020 The Chromium Authors. All rights reserved.
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/utils.h"

namespace liburlpattern {

namespace {
constexpr absl::string_view kSpecialCharacters(".+*?=^!:${}()[]|/\\");
}  // namespace

size_t EscapedLength(absl::string_view input) {
  size_t count = input.size();
  for (auto& c : input) {
    if (kSpecialCharacters.find(c) != std::string::npos)
      count += 1;
  }
  return count;
}

void EscapeStringAndAppend(absl::string_view input,
                           std::string& append_target) {
  for (auto& c : input) {
    if (kSpecialCharacters.find(c) != std::string::npos)
      append_target += '\\';
    append_target += c;
  }
}

std::string EscapeString(absl::string_view input) {
  std::string result;
  result.reserve(EscapedLength(input));
  EscapeStringAndAppend(input, result);
  return result;
}

}  // namespace liburlpattern
