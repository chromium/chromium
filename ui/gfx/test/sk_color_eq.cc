// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/test/sk_color_eq.h"

#include <iomanip>
#include <string>

namespace gfx {

namespace {

std::string ColorAsString(SkColor color) {
  std::ostringstream stream;
  stream << std::hex << std::uppercase << "#" << std::setfill('0')
         << std::setw(2) << SkColorGetA(color) << std::setw(2)
         << SkColorGetR(color) << std::setw(2) << SkColorGetG(color)
         << std::setw(2) << SkColorGetB(color);
  return stream.str();
}

}  // namespace

::testing::AssertionResult AssertSkColorsEqual(const char* lhs_expr,
                                               const char* rhs_expr,
                                               SkColor lhs,
                                               SkColor rhs) {
  if (lhs == rhs) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
         << "Expected equality of these values:\n"
         << lhs_expr << "\n    Which is: " << ColorAsString(lhs) << "\n"
         << rhs_expr << "\n    Which is: " << ColorAsString(rhs);
}

}  // namespace gfx
