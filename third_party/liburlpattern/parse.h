// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef THIRD_PARTY_LIBURLPATTERN_PARSE_H_
#define THIRD_PARTY_LIBURLPATTERN_PARSE_H_

#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"
#include "third_party/liburlpattern/options.h"

// NOTE: This code is a work-in-progress.  It is not ready for production use.

namespace liburlpattern {

class Pattern;

// Parse a pattern string and return the result.  The input |pattern| must
// consist of ASCII characters.  Any non-ASCII characters should be UTF-8
// encoded and % escaped, similar to URLs, prior to calling this function.
// An |options| value may be provided to override default behavior.
COMPONENT_EXPORT(LIBURLPATTERN)
absl::StatusOr<Pattern> Parse(absl::string_view pattern,
                              const Options& options = Options());

}  // namespace liburlpattern

#endif  // THIRD_PARTY_LIBURLPATTERN_PARSE_H_
