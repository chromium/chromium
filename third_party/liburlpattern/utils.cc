// Copyright 2020 The Chromium Authors. All rights reserved.
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/utils.h"

namespace liburlpattern {

std::string EscapeString(absl::string_view input) {
  std::string result;
  result.reserve(input.size());
  const absl::string_view special_characters(".+*?=^!:${}()[]|/\\");
  for (auto& c : input) {
    if (special_characters.find(c) != std::string::npos)
      result += '\\';
    result += c;
  }
  return result;
}

}  // namespace liburlpattern
