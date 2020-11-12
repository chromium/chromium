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

// Escape an input string so that it may be safely included in a
// regular expression.
COMPONENT_EXPORT(LIBURLPATTERN)
std::string EscapeString(absl::string_view input);

}  // namespace liburlpattern

#endif  // THIRD_PARTY_LIBURLPATTERN_UTILS_H_
