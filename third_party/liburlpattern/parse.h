// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef THIRD_PARTY_LIBURLPATTERN_PARSE_H_
#define THIRD_PARTY_LIBURLPATTERN_PARSE_H_

#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

// NOTE: This code is a work-in-progress.  It is not ready for production use.

namespace liburlpattern {

class Pattern;

// Parse a pattern string and return the result.  The input |pattern| must
// consist of ASCII characters.  Any non-ASCII characters should be UTF-8
// encoded and % escaped, similar to URLs, prior to calling this function.
// |delimiter_list| contains a list of characters that are considered segment
// separators when performing a kSegmentWildcard.  This is the behavior you
// get when you specify a name `:foo` without a custom regular expression.
// The |prefix_list| contains a list of characters to automatically treat
// as a prefix when they appear before a kName or kRegex Token; e.g. "/:foo",
// includes the leading "/" as the prefix for the "foo" named group by default.
COMPONENT_EXPORT(LIBURLPATTERN)
absl::StatusOr<Pattern> Parse(absl::string_view pattern,
                              absl::string_view delimiter_list = "/#?",
                              absl::string_view prefix_list = "./");

}  // namespace liburlpattern

#endif  // THIRD_PARTY_LIBURLPATTERN_PARSE_H_
