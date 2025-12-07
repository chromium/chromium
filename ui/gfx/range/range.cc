// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/range/range.h"

#include <inttypes.h>

#include <ostream>

#include "base/check.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace gfx {

std::string Range::ToString() const {
  return base::StringPrintf("{%" PRIuS ",%" PRIuS "}", start(), end());
}

std::vector<int> Range::ToIntVector() const {
  CHECK(GetMax() <= static_cast<size_t>(std::numeric_limits<int>::max()));
  std::vector<int> indices(length());
  int index = is_reversed() ? GetMax() - 1 : GetMin();
  std::generate(indices.begin(), indices.end(),
                [&] { return is_reversed() ? index-- : index++; });
  return indices;
}

std::ostream& operator<<(std::ostream& os, const Range& range) {
  return os << range.ToString();
}

}  // namespace gfx
