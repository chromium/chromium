// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_fluent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string_view>
#include <utility>

#include "base/strings/strcat.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/record_paint_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
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
  const NativeThemeFluent& theme() const { return theme_; }

  // Mocks the availability of the font for drawing arrow icons.
  void SetArrowIconsAvailable(bool available) {
    theme_.SetArrowIconsAvailableForTesting(available);
    EXPECT_EQ(theme().GetArrowIconsAvailable(), available);
  }

  void VerifyArrowRect() const {
    SCOPED_TRACE(::testing::Message() << "Scale: " << ScaleFromDIP());
    for (const auto& parts_elem :
         std::to_array<std::pair<NativeTheme::Part, std::string_view>>(
             {{NativeTheme::kScrollbarDownArrow, "down"},
              {NativeTheme::kScrollbarLeftArrow, "left"},
              {NativeTheme::kScrollbarRightArrow, "right"},
              {NativeTheme::kScrollbarUpArrow, "up"}})) {
      const NativeTheme::Part part = parts_elem.first;
      SCOPED_TRACE(base::StrCat({"Arrow direction: ", parts_elem.second}));
      const gfx::RectF button_rect = ButtonRect(part);
      for (const auto& states_elem :
           std::to_array<std::pair<NativeTheme::State, std::string_view>>(
               {{NativeTheme::kDisabled, "disabled"},
                {NativeTheme::kHovered, "hovered"},
                {NativeTheme::kNormal, "normal"},
                {NativeTheme::kPressed, "pressed"}})) {
        const NativeTheme::State state = states_elem.first;
        SCOPED_TRACE(base::StrCat({"Arrow state: ", states_elem.second}));
        const gfx::RectF arrow_rect =
            theme().GetArrowRect(gfx::ToNearestRect(button_rect), part, state);
        VerifyArrowRectCommonDimensions(arrow_rect);
        if (!theme().GetArrowIconsAvailable()) {
          VerifyArrowRectIsIntRect(arrow_rect);
        }
        VerifyArrowRectIsCentered(button_rect, arrow_rect, part);
        VerifyArrowRectLengthRatio(button_rect, arrow_rect, state);
      }
    }
  }

  void PaintScrollbarThumb(cc::PaintCanvas* canvas) const {
    ColorProvider color_provider;
    theme_.PaintScrollbarThumb(canvas, &color_provider,
                               NativeTheme::kScrollbarVerticalThumb,
                               NativeTheme::kNormal, gfx::Rect(15, 100), {});
  }

 private:
  static float ScaleFromDIP() { return GetParam(); }

  static void VerifyArrowRectCommonDimensions(const gfx::RectF& arrow_rect) {
    EXPECT_FALSE(arrow_rect.IsEmpty());
    EXPECT_EQ(arrow_rect.width(), arrow_rect.height());
    EXPECT_EQ(arrow_rect.width(), std::floor(arrow_rect.width()));
  }

  void VerifyArrowRectIsCentered(const gfx::RectF& button_rect,
                                 const gfx::RectF& arrow_rect,
                                 NativeTheme::Part part) const {
    const gfx::PointF button_center = button_rect.CenterPoint();
    const gfx::PointF arrow_center = arrow_rect.CenterPoint();
    // The arrow is shifted away from center along the length axis by one dp,
    // rounded to integral px.
    float expected_shift = std::round(ScaleFromDIP());
    if (!theme().GetArrowIconsAvailable()) {
      // For triangular arrows, rect coordinates are snapped to integers, which
      // may introduce an additional half pixel shift.
      expected_shift += 0.5f;
    }
    if (part == NativeTheme::kScrollbarUpArrow ||
        part == NativeTheme::kScrollbarDownArrow) {
      EXPECT_EQ(button_center.x(), arrow_center.x());
      EXPECT_NEAR(button_center.y(), arrow_center.y(), expected_shift);
    } else {
      EXPECT_NEAR(button_center.x(), arrow_center.x(), expected_shift);
      EXPECT_EQ(button_center.y(), arrow_center.y());
    }
  }

  static void VerifyArrowRectIsIntRect(const gfx::RectF& arrow_rect) {
    EXPECT_TRUE(gfx::IsNearestRectWithinDistance(arrow_rect, 0.01f));
  }

  static void VerifyArrowRectLengthRatio(const gfx::RectF& button_rect,
                                         const gfx::RectF& arrow_rect,
                                         NativeTheme::State state) {
    const float thickness = std::min(button_rect.width(), button_rect.height());
    const float arrow_side = arrow_rect.width();  // The arrow is square.
    if (state == NativeTheme::kPressed) {
      // Pressed icons are ~0.5 times as thick as the button (precise value
      // depends on zoom and whether arrow icons are available).
      EXPECT_GT(arrow_side, thickness / 3.0f);
      EXPECT_LT(arrow_side, thickness / 1.5f);
    } else {
      // Non-pressed arrows are ~0.6 times as thick as the button.
      EXPECT_GT(arrow_side, thickness / 2.0f);
      EXPECT_LT(arrow_side, thickness);
    }
  }

  gfx::RectF ButtonRect(NativeTheme::Part part) const {
    gfx::Rect rect({}, theme().GetVerticalScrollbarButtonSize());
    if (part == NativeTheme::kScrollbarLeftArrow ||
        part == NativeTheme::kScrollbarRightArrow) {
      rect.Transpose();
    }
    return gfx::RectF(gfx::ScaleToEnclosedRect(rect, ScaleFromDIP()));
  }

 private:
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
            theme().GetScrollbarThumbColor(color_provider.get(),
                                           NativeTheme::kNormal, {}));
  const auto hovered_thumb_color =
      color_provider->GetColor(kColorWebNativeControlScrollbarThumbHovered);
  EXPECT_EQ(hovered_thumb_color,
            theme().GetScrollbarThumbColor(color_provider.get(),
                                           NativeTheme::kHovered, {}));
  const auto pressed_thumb_color =
      color_provider->GetColor(kColorWebNativeControlScrollbarThumbPressed);
  EXPECT_EQ(pressed_thumb_color,
            theme().GetScrollbarThumbColor(color_provider.get(),
                                           NativeTheme::kPressed, {}));

  // When the thumb is being painted in minimal mode, the normal state should
  // return the minimal mode's transparent color while the other states remain
  // unaffected.
  static constexpr NativeTheme::ScrollbarThumbExtraParams kMinimalParams = {
      .is_thumb_minimal_mode = true};
  EXPECT_EQ(color_provider->GetColor(
                kColorWebNativeControlScrollbarThumbOverlayMinimalMode),
            theme().GetScrollbarThumbColor(
                color_provider.get(), NativeTheme::kNormal, kMinimalParams));
  EXPECT_EQ(hovered_thumb_color,
            theme().GetScrollbarThumbColor(
                color_provider.get(), NativeTheme::kHovered, kMinimalParams));
  EXPECT_EQ(pressed_thumb_color,
            theme().GetScrollbarThumbColor(
                color_provider.get(), NativeTheme::kPressed, kMinimalParams));

  // When there is a css color set in the extra params, we modify the color
  // when it is hovered or pressed to signal the change in state.
  static constexpr auto kCssColor = SK_ColorGREEN;
  static constexpr NativeTheme::ScrollbarThumbExtraParams kColorParams = {
      .thumb_color = kCssColor};
  EXPECT_EQ(kCssColor,
            theme().GetScrollbarThumbColor(color_provider.get(),
                                           NativeTheme::kNormal, kColorParams));
  EXPECT_NE(kCssColor,
            theme().GetScrollbarThumbColor(
                color_provider.get(), NativeTheme::kHovered, kColorParams));
  EXPECT_NE(kCssColor,
            theme().GetScrollbarThumbColor(
                color_provider.get(), NativeTheme::kPressed, kColorParams));
}

INSTANTIATE_TEST_SUITE_P(All,
                         NativeThemeFluentTest,
                         ::testing::Values(1.0f, 1.25f, 1.5f, 1.75f, 2.0f));

}  // namespace ui
