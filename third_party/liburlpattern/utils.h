// Copyright 2020 The Chromium Authors. All rights reserved.
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef THIRD_PARTY_LIBURLPATTERN_UTILS_H_
#define THIRD_PARTY_LIBURLPATTERN_UTILS_H_

#include <string>
#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace liburlpattern {

// The "full wildcard" regex pattern.  This regex value is treated specially
// resulting in a kFullWildcard Part instead of a kRegex Part.
constexpr const char* kFullWildcardRegex = ".*";

// Return the expected length of the value returned by EscapeString().
COMPONENT_EXPORT(LIBURLPATTERN)
size_t EscapedRegexpStringLength(absl::string_view input);

// Escape an input string so that it may be safely included in a
// regular expression.
COMPONENT_EXPORT(LIBURLPATTERN)
std::string EscapeRegexpString(absl::string_view input);

// Escape the input string so that it may be safely included in a
// regular expression and append the result directly to the given target.
void EscapeRegexpStringAndAppend(absl::string_view input,
                                 std::string& append_target);

// Escape a fixed input string so that it may be safely included in a
// pattern string.  Appends the result directly to the given target.
COMPONENT_EXPORT(LIBURLPATTERN)
void EscapePatternStringAndAppend(absl::string_view input,
                                  std::string& append_target);

}  // namespace liburlpattern

#endif  // THIRD_PARTY_LIBURLPATTERN_UTILS_H_
