// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/range/range.h"

#include <inttypes.h>

#include <ostream>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace gfx {

std::string Range::ToString() const {
  return base::StringPrintf("{%" PRIuS ",%" PRIuS "}", start(), end());
}

std::ostream& operator<<(std::ostream& os, const Range& range) {
  return os << range.ToString();
}

}  // namespace gfx
