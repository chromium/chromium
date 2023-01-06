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

bool ColorComponentsClose(SkColor component1,
                          SkColor component2,
                          int max_deviation) {
  int c1 = static_cast<int>(component1);
  int c2 = static_cast<int>(component2);
  return std::abs(c1 - c2) <= max_deviation;
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

bool ColorsClose(SkColor color1,
                 SkColor color2,
                 int max_per_channel_deviation) {
  return ColorComponentsClose(SkColorGetR(color1), SkColorGetR(color2),
                              max_per_channel_deviation) &&
         ColorComponentsClose(SkColorGetG(color1), SkColorGetG(color2),
                              max_per_channel_deviation) &&
         ColorComponentsClose(SkColorGetB(color1), SkColorGetB(color2),
                              max_per_channel_deviation) &&
         ColorComponentsClose(SkColorGetA(color1), SkColorGetA(color2),
                              max_per_channel_deviation);
}

::testing::AssertionResult AssertSkColorsClose(
    const char* lhs_expr,
    const char* rhs_expr,
    const char* max_per_channel_deviation_expr,
    SkColor lhs,
    SkColor rhs,
    int max_per_channel_deviation) {
  if (ColorsClose(lhs, rhs, max_per_channel_deviation)) {
    return ::testing::AssertionSuccess();
  }

  return ::testing::AssertionFailure()
         << "Expected closeness of these values:\n"
         << lhs_expr << "\n    Which is: " << ColorAsString(lhs) << "\n"
         << rhs_expr << "\n    Which is: " << ColorAsString(rhs) << "\n"
         << max_per_channel_deviation_expr << " (max per-channel deviation)"
         << "\n    Which is: " << max_per_channel_deviation;
}

}  // namespace gfx
