// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_base.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/native_theme/native_theme.h"

namespace ui {
namespace {

void VerifyTriangle(SkPath path, base::span<const SkPoint> expected_points) {
  ASSERT_EQ(static_cast<size_t>(path.countPoints()), expected_points.size());
  for (int i = 0; const SkPoint& expected_point : expected_points) {
    const SkPoint actual_point = path.getPoint(i++);
    // TODO(pkasting): Move `cc::PaintOpHelper::ToString(const SkPoint&)` to a
    // more general location and use `EXPECT_EQ(actual_point, expected_point)`.
    EXPECT_EQ(actual_point.x(), expected_point.x());
    EXPECT_EQ(actual_point.y(), expected_point.y());
  }
}

}  // namespace

class NativeThemeBaseTest : public ::testing::Test {
 protected:
  gfx::RectF GetArrowRect(const gfx::Rect& rect) const {
    return theme_.GetArrowRect(rect, NativeTheme::kScrollbarDownArrow,
                               NativeTheme::kNormal);
  }

  SkPath PathForArrow(const gfx::Rect& rect, NativeTheme::Part part) const {
    return NativeThemeBase::PathForArrow(GetArrowRect(rect), part);
  }

  float GetScrollbarPartContrastRatio() const {
    return theme_.GetScrollbarPartContrastRatioForState(
        NativeTheme::State::kHovered);
  }

  SkColor GetContrastingColorForScrollbarPart(
      std::optional<SkColor> color,
      std::optional<SkColor> bg_color) const {
    return theme_
        .GetContrastingColorForScrollbarPart(
            std::move(color), std::move(bg_color), NativeTheme::State::kHovered)
        .value();
  }

 private:
  NativeThemeBase theme_;
};

TEST_F(NativeThemeBaseTest, BoundingRectSquare) {
  EXPECT_EQ(GetArrowRect(gfx::Rect(42, 61, 21, 21)), gfx::RectF(48, 67, 9, 9));
}

TEST_F(NativeThemeBaseTest, BoundingRectSlightlyRectangular) {
  // Stretched horizontally.
  EXPECT_EQ(GetArrowRect(gfx::Rect(42, 61, 25, 20)),
            gfx::RectF(49, 66, 11, 11));

  // Stretched vertically.
  EXPECT_EQ(GetArrowRect(gfx::Rect(42, 61, 14, 10)), gfx::RectF(46, 63, 6, 6));
}

TEST_F(NativeThemeBaseTest, BoundingRectVeryRectangular) {
  // Stretched horizontally.
  EXPECT_EQ(GetArrowRect(gfx::Rect(42, 61, 30, 8)), gfx::RectF(53, 61, 8, 8));

  // Stretched vertically.
  EXPECT_EQ(GetArrowRect(gfx::Rect(42, 61, 6, 44)), gfx::RectF(42, 80, 6, 6));
}

TEST_F(NativeThemeBaseTest, BoundingRectSnappedToWholePixels) {
  EXPECT_EQ(GetArrowRect(gfx::Rect(0, 0, 9, 10)).x(), 3);
  EXPECT_EQ(GetArrowRect(gfx::Rect(0, 0, 10, 9)).y(), 3);
}

TEST_F(NativeThemeBaseTest, VerticalArrows) {
  // Up arrow, sized for 1x.
  VerifyTriangle(
      PathForArrow(gfx::Rect(100, 200, 17, 17), NativeTheme::kScrollbarUpArrow),
      {SkPoint::Make(105, 211), SkPoint::Make(112, 211),
       SkPoint::Make(108.5, 207)});

  // 1.25x, should be larger.
  VerifyTriangle(
      PathForArrow(gfx::Rect(50, 70, 21, 21), NativeTheme::kScrollbarUpArrow),
      {SkPoint::Make(56, 84), SkPoint::Make(65, 84), SkPoint::Make(60.5, 79)});

  // Down arrow is just a flipped up arrow.
  VerifyTriangle(
      PathForArrow(gfx::Rect(20, 80, 17, 17), NativeTheme::kScrollbarDownArrow),
      {SkPoint::Make(25, 86), SkPoint::Make(32, 86), SkPoint::Make(28.5, 90)});
}

TEST_F(NativeThemeBaseTest, HorizontalArrows) {
  // Right arrow, sized for 1x.
  VerifyTriangle(PathForArrow(gfx::Rect(100, 200, 17, 17),
                              NativeTheme::kScrollbarRightArrow),
                 {SkPoint::Make(107, 205), SkPoint::Make(107, 212),
                  SkPoint::Make(111, 208.5)});

  // Button size for 1.25x, should be larger.
  VerifyTriangle(
      PathForArrow(gfx::Rect(50, 70, 21, 21),
                   NativeTheme::kScrollbarRightArrow),
      {SkPoint::Make(58, 76), SkPoint::Make(58, 85), SkPoint::Make(63, 80.5)});

  // Left arrow is just a flipped right arrow.
  VerifyTriangle(
      PathForArrow(gfx::Rect(20, 80, 17, 17), NativeTheme::kScrollbarLeftArrow),
      {SkPoint::Make(30, 85), SkPoint::Make(30, 92), SkPoint::Make(26, 88.5)});
}

TEST_F(NativeThemeBaseTest, ArrowForNonSquareButton) {
  VerifyTriangle(
      PathForArrow(gfx::Rect(90, 80, 42, 37), NativeTheme::kScrollbarLeftArrow),
      {SkPoint::Make(116, 89), SkPoint::Make(116, 109),
       SkPoint::Make(105, 99)});
}

// Checks that `GetContrastingColorForScrollbarPart()` doesn't modify
// fully-transparent colors.
TEST_F(NativeThemeBaseTest, GetContrastingColorTransparent) {
  static constexpr auto kOpaqueColor = SkColorSetRGB(0xBA, 0x74, 0x74);
  static constexpr auto kTransparentColor = SkColorSetA(kOpaqueColor, 0x00);
  EXPECT_EQ(kTransparentColor, GetContrastingColorForScrollbarPart(
                                   kTransparentColor, kOpaqueColor));
}

// Checks that `GetContrastingColorForScrollbarPart` can adapt to a whole range
// of luminosity for the colors to modify.
TEST_F(NativeThemeBaseTest, GetContrastingColorLuminosity) {
  for (unsigned i = 0; i < 255; i++) {
    const SkColor color = SkColorSetRGB(i, i, i);
    const float luminance = color_utils::GetRelativeLuminance(color);
    const float adjusted_luminance = color_utils::GetRelativeLuminance(
        GetContrastingColorForScrollbarPart(color, std::nullopt));
    if (color_utils::IsDark(color)) {
      EXPECT_GT(adjusted_luminance, luminance);
    } else {
      EXPECT_LT(adjusted_luminance, luminance);
    }
  }
}

// Checks that the returned color never loses contrast against the background
// color.
TEST_F(NativeThemeBaseTest, GetContrastingColorBackgroundContrast) {
  for (size_t c = 0; c < 24; c += 8) {
    // Test with Green, Blue and Red.
    const SkColor bg_color =
        SkColorSetA(SkColor(uint32_t{0x80} << c), SK_AlphaOPAQUE);
    for (uint32_t i = 0; i < 255; ++i) {
      const SkColor adjusted_color = GetContrastingColorForScrollbarPart(
          SkColorSetA(SkColor(i << c), SK_AlphaOPAQUE), bg_color);
      EXPECT_GT(color_utils::GetContrastRatio(bg_color, adjusted_color),
                GetScrollbarPartContrastRatio());
    }
  }
}

// Checks that grayscale colors never lose contrast with the background when it
// is different shades of gray.
TEST_F(NativeThemeBaseTest, GetContrastingColorGrayScales) {
  static constexpr auto kColors =
      std::to_array({SK_ColorWHITE, SK_ColorLTGRAY, SK_ColorGRAY,
                     SK_ColorDKGRAY, SK_ColorBLACK});
  for (const auto foreground : kColors) {
    for (const auto background : kColors) {
      const float background_contrast_ratio = color_utils::GetContrastRatio(
          background,
          GetContrastingColorForScrollbarPart(foreground, background));
      const float base_contrast_ratio = GetScrollbarPartContrastRatio();
      EXPECT_GE(background_contrast_ratio, base_contrast_ratio);
    }
  }
}

// Checks that transparent foreground colors don't lose contrast against the
// background when altered.
TEST_F(NativeThemeBaseTest, GetContrastingColorTransparencies) {
  for (const auto background : {SK_ColorBLACK, SK_ColorWHITE}) {
    for (auto foreground : {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE}) {
      for (int i = 1; i < 4; ++i) {
        foreground = SkColorSetA(foreground, SK_AlphaOPAQUE * i / 4);
        const float background_contrast_ratio = color_utils::GetContrastRatio(
            background,
            GetContrastingColorForScrollbarPart(foreground, background));
        const float base_contrast_ratio = GetScrollbarPartContrastRatio();
        EXPECT_GE(background_contrast_ratio, base_contrast_ratio);
      }
    }
  }
}

}  // namespace ui
