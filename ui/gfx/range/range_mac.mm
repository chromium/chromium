// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/range/range.h"

#include <stddef.h>

#include <limits>

#include "base/check_op.h"
#include "base/numerics/checked_math.h"

namespace gfx {

Range::Range(const NSRange& range) {
  *this = range;
}

Range Range::FromPossiblyInvalidNSRange(const NSRange& range) {
  uint32_t end;
  if (range.location == NSNotFound ||
      !base::IsValueInRangeForNumericType<uint32_t>(range.location) ||
      !base::IsValueInRangeForNumericType<uint32_t>(range.length) ||
      !base::CheckAdd<uint32_t>(range.location, range.length)
           .AssignIfValid(&end)) {
    return InvalidRange();
  }

  return Range(range.location, end);
}

Range& Range::operator=(const NSRange& range) {
  if (range.location == NSNotFound) {
    DCHECK_EQ(0U, range.length);
    *this = InvalidRange();
  } else {
    set_start(range.location);
    set_end(start() + range.length);
  }
  return *this;
}

NSRange Range::ToNSRange() const {
  if (!IsValid())
    return NSMakeRange(NSNotFound, 0);
  return NSMakeRange(GetMin(), length());
}

}  // namespace gfx
