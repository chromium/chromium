// Copyright 2020 The Chromium Authors. All rights reserved.
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/parse.h"

#include <string>
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/liburlpattern/pattern.h"

namespace liburlpattern {

namespace {

bool IsValidChar(char c) {
  // Characters should be valid ASCII code points:
  // https://infra.spec.whatwg.org/#ascii-code-point
  return c >= 0x0000 && c <= 0x007f;
}

}  // namespace

absl::StatusOr<Pattern> Parse(absl::string_view pattern) {
  for (char c : pattern) {
    if (!IsValidChar(c)) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Invalid character '%c' in pattern '%s'.", c, pattern));
    }

    // TODO: Implement actual pattern parsing.
  }

  return Pattern();
}

}  // namespace liburlpattern
