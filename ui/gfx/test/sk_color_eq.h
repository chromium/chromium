// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TEST_SK_COLOR_EQ_H_
#define UI_GFX_TEST_SK_COLOR_EQ_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {

#define EXPECT_SKCOLOR_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertSkColorsEqual, a, b)

::testing::AssertionResult AssertSkColorsEqual(const char* lhs_expr,
                                               const char* rhs_expr,
                                               SkColor lhs,
                                               SkColor rhs);

bool ColorsClose(SkColor color1, SkColor color2, int max_per_channel_deviation);

// Macro for comparing the SkColors that is tolerant of floating point rounding
// and lossy color space conversions.
#define EXPECT_SKCOLOR_CLOSE(a, b, max_per_channel_deviation) \
  EXPECT_PRED_FORMAT3(::gfx::AssertSkColorsClose, a, b,       \
                      max_per_channel_deviation)

::testing::AssertionResult AssertSkColorsClose(
    const char* lhs_expr,
    const char* rhs_expr,
    const char* max_per_channel_deviation_expr,
    SkColor lhs,
    SkColor rhs,
    int max_per_channel_deviation);

}  // namespace gfx

#endif  // UI_GFX_TEST_SK_COLOR_EQ_H_
