// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/range/range_f.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <ostream>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace gfx {

RangeF RangeF::Intersect(const Range& range) const {
  RangeF range_f(range.start(), range.end());
  return Intersect(range_f);
}

std::string RangeF::ToString() const {
  return base::StringPrintf("{%f,%f}", start(), end());
}

std::ostream& operator<<(std::ostream& os, const RangeF& range) {
  return os << range.ToString();
}

}  // namespace gfx
