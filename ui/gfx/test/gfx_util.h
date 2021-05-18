// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TEST_GFX_UTIL_H_
#define UI_GFX_TEST_GFX_UTIL_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"

namespace gfx {

class AxisTransform2d;
class BoxF;
class PointF;
class RectF;
class SizeF;

// Tests should use this scoped setter, instead of calling
// SetDefaultFontDescription directly.
class ScopedDefaultFontDescription {
 public:
  explicit ScopedDefaultFontDescription(const std::string& font_description) {
    FontList::SetDefaultFontDescription(font_description);
  }
  ~ScopedDefaultFontDescription() {
    FontList::SetDefaultFontDescription(std::string());
  }
};

#define EXPECT_AXIS_TRANSFORM2D_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertAxisTransform2dFloatEqual, a, b)

::testing::AssertionResult AssertAxisTransform2dFloatEqual(
    const char* lhs_expr,
    const char* rhs_expr,
    const AxisTransform2d& lhs,
    const AxisTransform2d& rhs);

#define EXPECT_BOXF_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertBoxFloatEqual, a, b)

::testing::AssertionResult AssertBoxFloatEqual(const char* lhs_expr,
                                               const char* rhs_expr,
                                               const BoxF& lhs,
                                               const BoxF& rhs);

#define EXPECT_POINTF_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertPointFloatEqual, a, b)

::testing::AssertionResult AssertPointFloatEqual(const char* lhs_expr,
                                                 const char* rhs_expr,
                                                 const PointF& lhs,
                                                 const PointF& rhs);

#define EXPECT_RECTF_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertRectFloatEqual, a, b)

::testing::AssertionResult AssertRectFloatEqual(const char* lhs_expr,
                                                const char* rhs_expr,
                                                const RectF& lhs,
                                                const RectF& rhs);

#define EXPECT_SKCOLOR_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertSkColorsEqual, a, b)

::testing::AssertionResult AssertSkColorsEqual(const char* lhs_expr,
                                               const char* rhs_expr,
                                               SkColor lhs,
                                               SkColor rhs);

#define EXPECT_SIZEF_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertSizeFFloatEqual, a, b)

::testing::AssertionResult AssertSizeFFloatEqual(const char* lhs_expr,
                                                 const char* rhs_expr,
                                                 const SizeF& lhs,
                                                 const SizeF& rhs);
}  // namespace gfx

#endif  // UI_GFX_TEST_GFX_UTIL_H_
