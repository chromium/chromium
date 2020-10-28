// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef THIRD_PARTY_LIBURLPATTERN_PATTERN_H_
#define THIRD_PARTY_LIBURLPATTERN_PATTERN_H_

#include "base/component_export.h"

// NOTE: This code is a work-in-progress.  It is not ready for production use.

namespace liburlpattern {

// This class represents a successfully parsed pattern string.  It will contain
// an intermediate representation that can be used to generate either a regular
// expression string or to directly match against input strings.  Not all
// patterns are supported for direct matching.
class COMPONENT_EXPORT(LIBURLPATTERN) Pattern {
  // TODO: Implement pattern details.
};

}  // namespace liburlpattern

#endif  // THIRD_PARTY_LIBURLPATTERN_PATTERN_H_
