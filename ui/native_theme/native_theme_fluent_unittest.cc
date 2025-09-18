// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_fluent.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "cc/paint/paint_op.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/record_paint_canvas.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

class NativeThemeFluentTest : public ::testing::Test,
                              public ::testing::WithParamInterface<float> {
 protected:
  void VerifyArrowRectCommonDimensions(const gfx::RectF& arrow_rect) const {
    EXPECT_FALSE(arrow_rect.IsEmpty());
    EXPECT_EQ(arrow_rect.width(), arrow_rect.height());
    EXPECT_EQ(arrow_rect.width(), std::floor(arrow_rect.width()));
  }

  void VerifyArrowRectIsCentered(const gfx::RectF& button_rect,
                                 const gfx::RectF& arrow_rect,
                                 NativeTheme::Part part) const {
    if (part == NativeTheme::kScrollbarUpArrow ||
        part == NativeTheme::kScrollbarDownArrow) {
      EXPECT_EQ(button_rect.CenterPoint().x(), arrow_rect.CenterPoint().x());
      // Due to the offset the arrow rect is shifted from the center.
      // See NativeThemeFluent::GetArrowRect() for more details. Same below.
      EXPECT_NEAR(button_rect.CenterPoint().y(), arrow_rect.CenterPoint().y(),
                  ScaleFromDIP() * 2);
    } else {
      EXPECT_EQ(button_rect.CenterPoint().y(), arrow_rect.CenterPoint().y());
      EXPECT_NEAR(button_rect.CenterPoint().x(), arrow_rect.CenterPoint().x(),
                  ScaleFromDIP() * 2);
    }
  }

  void VerifyArrowRectIsIntRect(const gfx::RectF& arrow_rect) const {
    if (theme_.ArrowIconsAvailable()) {
      return;
    }

    // Verify that an arrow rect with triangular arrows is an integer rect.
    EXPECT_TRUE(IsNearestRectWithinDistance(arrow_rect, 0.01f));
  }

  void VerifyArrowRectLengthRatio(const gfx::RectF& button_rect,
                                  const gfx::RectF& arrow_rect,
                                  NativeTheme::State state) const {
    const int smaller_button_side =
        std::min(button_rect.width(), button_rect.height());
    if (state == NativeTheme::kNormal) {
      // Default state arrows are slightly bigger than the half of the button's
      // smaller side (track thickness).
      EXPECT_GT(arrow_rect.width(), smaller_button_side / 2.0f);
      EXPECT_LT(arrow_rect.width(), smaller_button_side);
    } else {
      EXPECT_GT(arrow_rect.width(), smaller_button_side / 3.0f);
      EXPECT_LT(arrow_rect.width(), smaller_button_side / 1.5f);
    }
  }

  void VerifyArrowRect() const {
    for (auto const& part :
         {NativeTheme::kScrollbarUpArrow, NativeTheme::kScrollbarLeftArrow}) {
      const gfx::RectF button_rect(ButtonRect(part));
      for (auto const& state : {NativeTheme::kNormal, NativeTheme::kPressed}) {
        const gfx::RectF arrow_rect =
            theme_.GetArrowRect(ToNearestRect(button_rect), part, state);
        VerifyArrowRectCommonDimensions(arrow_rect);
        VerifyArrowRectIsIntRect(arrow_rect);
        VerifyArrowRectIsCentered(button_rect, arrow_rect, part);
        VerifyArrowRectLengthRatio(button_rect, arrow_rect, state);
      }
    }
  }

  gfx::RectF ButtonRect(NativeTheme::Part part) const {
    const int button_length = base::ClampFloor(
        NativeThemeFluent::kScrollbarButtonSideLength * ScaleFromDIP());
    const int track_thickness = base::ClampFloor(
        NativeThemeFluent::kScrollbarThickness * ScaleFromDIP());

    if (part == NativeTheme::kScrollbarUpArrow ||
        part == NativeTheme::kScrollbarDownArrow) {
      return gfx::RectF(0, 0, track_thickness, button_length);
    }

    return gfx::RectF(0, 0, button_length, track_thickness);
  }

  void PaintScrollbarThumb(cc::PaintCanvas* canvas) const {
    ColorProvider color_provider;
    theme_.PaintScrollbarThumb(canvas, &color_provider,
                               NativeTheme::kScrollbarVerticalThumb,
                               NativeTheme::kNormal, gfx::Rect(15, 100), {});
  }

  float ScaleFromDIP() const { return GetParam(); }

  // Mocks the availability of the font for drawing arrow icons.
  void SetArrowIconsAvailable(bool enabled) {
    if (enabled) {
      theme_.typeface_ = skia::DefaultTypeface();
      EXPECT_TRUE(theme_.ArrowIconsAvailable());
    } else {
      theme_.typeface_ = nullptr;
      EXPECT_FALSE(theme_.ArrowIconsAvailable());
    }
  }

  NativeThemeFluent theme_;
};

// Verify the dimensions of an arrow rect with triangular arrows for a given
// button rect depending on the arrow direction and state.
TEST_P(NativeThemeFluentTest, VerifyArrowRectWithTriangularArrows) {
  SetArrowIconsAvailable(false);
  VerifyArrowRect();
}

// Verify the dimensions of an arrow rect with arrow icons for a given button
// rect depending on the arrow direction and state.
TEST_P(NativeThemeFluentTest, VerifyArrowRectWithArrowIcons) {
  SetArrowIconsAvailable(true);
  VerifyArrowRect();
}

// Verify that the thumb paint function draws a round rectangle. Generally,
// `NativeThemeFluent::Paint*()` functions are covered by Blink's web tests; but
// in web tests we render the thumbs as squares instead of pill-shaped. This
// test ensures we don't lose coverage on the PaintOp called to draw the thumb.
TEST_F(NativeThemeFluentTest, PaintThumbRoundedCorners) {
  cc::RecordPaintCanvas canvas;
  PaintScrollbarThumb(&canvas);
  EXPECT_EQ(canvas.TotalOpCount(), 1u);
  EXPECT_EQ(canvas.ReleaseAsRecord().GetFirstOp().GetType(),
            cc::PaintOpType::kDrawRRect);
}

// Verify that GetThumbColor returns the correct color given the scrollbar state
// and extra params.
TEST_F(NativeThemeFluentTest, GetThumbColor) {
  const std::unique_ptr<ColorProvider> color_provider =
      CreateDefaultColorProviderForBlink(/*dark_mode=*/false);

  // When there are no extra params set, the colors should be the ones that
  // correspond to the ColorId.
  EXPECT_EQ(color_provider->GetColor(kColorWebNativeControlScrollbarThumb),
            theme_.GetScrollbarThumbColor(color_provider.get(),
                                          NativeTheme::kNormal, {}));
  const auto hovered_thumb_color =
      color_provider->GetColor(kColorWebNativeControlScrollbarThumbHovered);
  EXPECT_EQ(hovered_thumb_color,
            theme_.GetScrollbarThumbColor(color_provider.get(),
                                          NativeTheme::kHovered, {}));
  const auto pressed_thumb_color =
      color_provider->GetColor(kColorWebNativeControlScrollbarThumbPressed);
  EXPECT_EQ(pressed_thumb_color,
            theme_.GetScrollbarThumbColor(color_provider.get(),
                                          NativeTheme::kPressed, {}));

  // When the thumb is being painted in minimal mode, the normal state should
  // return the minimal mode's transparent color while the other states remain
  // unaffected.
  static constexpr NativeTheme::ScrollbarThumbExtraParams kMinimalParams = {
      .is_thumb_minimal_mode = true};
  EXPECT_EQ(color_provider->GetColor(
                kColorWebNativeControlScrollbarThumbOverlayMinimalMode),
            theme_.GetScrollbarThumbColor(
                color_provider.get(), NativeTheme::kNormal, kMinimalParams));
  EXPECT_EQ(hovered_thumb_color,
            theme_.GetScrollbarThumbColor(
                color_provider.get(), NativeTheme::kHovered, kMinimalParams));
  EXPECT_EQ(pressed_thumb_color,
            theme_.GetScrollbarThumbColor(
                color_provider.get(), NativeTheme::kPressed, kMinimalParams));

  // When there is a css color set in the extra params, we modify the color
  // when it is hovered or pressed to signal the change in state.
  static constexpr auto kCssColor = SK_ColorGREEN;
  static constexpr NativeTheme::ScrollbarThumbExtraParams kColorParams = {
      .thumb_color = kCssColor};
  EXPECT_EQ(kCssColor,
            theme_.GetScrollbarThumbColor(color_provider.get(),
                                          NativeTheme::kNormal, kColorParams));
  EXPECT_NE(kCssColor,
            theme_.GetScrollbarThumbColor(color_provider.get(),
                                          NativeTheme::kHovered, kColorParams));
  EXPECT_NE(kCssColor,
            theme_.GetScrollbarThumbColor(color_provider.get(),
                                          NativeTheme::kPressed, kColorParams));
}

INSTANTIATE_TEST_SUITE_P(All,
                         NativeThemeFluentTest,
                         ::testing::Values(1.f, 1.25f, 1.5f, 1.75f, 2.f));

}  // namespace ui
