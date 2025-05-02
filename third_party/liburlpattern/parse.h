// Copyright 2020 The Chromium Authors
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef THIRD_PARTY_LIBURLPATTERN_PARSE_H_
#define THIRD_PARTY_LIBURLPATTERN_PARSE_H_

#include <string_view>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/liburlpattern/options.h"
#include "third_party/liburlpattern/utils.h"

namespace liburlpattern {

class Pattern;

// Parse a pattern string and return the result.  The parse will fail if the
// input |pattern| is not valid UTF-8.  Currently only group names may actually
// contain non-ASCII characters, however.  Unicode characters in other parts of
// the pattern will cause an error to be returned.  A |callback| must be
// provided to validate and encode plain text parts of the pattern.  An
// |options| value may be provided to override default behavior.
COMPONENT_EXPORT(LIBURLPATTERN)
base::expected<Pattern, absl::Status> Parse(std::string_view pattern,
                                            EncodeCallback callback,
                                            const Options& options = Options());

}  // namespace liburlpattern

#endif  // THIRD_PARTY_LIBURLPATTERN_PARSE_H_
