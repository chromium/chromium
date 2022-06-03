// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/range/range.h"

#include <stddef.h>

#include <limits>

#include "base/check_op.h"

namespace gfx {

Range::Range(const NSRange& range) {
  *this = range;
}

Range& Range::operator=(const NSRange& range) {
  if (range.location == NSNotFound) {
    DCHECK_EQ(0U, range.length);
    *this = InvalidRange();
  } else {
    set_start(range.location);
    // Don't overflow |end_|.
    DCHECK_LE(range.length, std::numeric_limits<size_t>::max() - start());
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
