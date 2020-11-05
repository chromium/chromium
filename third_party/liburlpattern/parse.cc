// Copyright 2020 The Chromium Authors. All rights reserved.
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/parse.h"

#include "third_party/liburlpattern/pattern.h"
#include "third_party/liburlpattern/tokenize.h"

namespace liburlpattern {

absl::StatusOr<Pattern> Parse(absl::string_view pattern) {
  auto result = Tokenize(pattern);
  if (!result.ok())
    return result.status();

  // TODO: Implement actual pattern parsing.

  return Pattern();
}

}  // namespace liburlpattern
