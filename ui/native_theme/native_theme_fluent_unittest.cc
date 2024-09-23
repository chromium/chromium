// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_fluent.h"

#include <memory>

#include "cc/paint/paint_flags.h"
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
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_constants_fluent.h"

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
      // See NativeThemeFluent::OffsetArrowRect() for more details. Same below.
      EXPECT_NEAR(button_rect.CenterPoint().y(), arrow_rect.CenterPoint().y(),
                  ScaleFromDIP() * 2);
    } else {
      EXPECT_EQ(button_rect.CenterPoint().y(), arrow_rect.CenterPoint().y());
      EXPECT_NEAR(button_rect.CenterPoint().x(), arrow_rect.CenterPoint().x(),
                  ScaleFromDIP() * 2);
    }
  }

  void VerifyArrowRectIsIntRect(const gfx::RectF& arrow_rect) const {
    if (theme_.ArrowIconsAvailable())
      return;

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
    const int button_length =
        base::ClampFloor(kFluentScrollbarButtonSideLength * ScaleFromDIP());
    const int track_thickness =
        base::ClampFloor(kFluentScrollbarThickness * ScaleFromDIP());

    if (part == NativeTheme::kScrollbarUpArrow ||
        part == NativeTheme::kScrollbarDownArrow)
      return gfx::RectF(0, 0, track_thickness, button_length);

    return gfx::RectF(0, 0, button_length, track_thickness);
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

  NativeThemeFluent theme_{false};
};

// Verify the dimensions of an arrow rect with triangular arrows for a given
// button rect depending on the arrow direction and state.
TEST_P(NativeThemeFluentTest, VerifyArrowRectWithTriangularArrows) {
  SetArrowIconsAvailable(false);
  VerifyArrowRect();
}

// Verify the dimensions of an arrow rect with arrow icons for a given
// button rect depending on the arrow direction and state.
TEST_P(NativeThemeFluentTest, VerifyArrowRectWithArrowIcons) {
  SetArrowIconsAvailable(true);
  VerifyArrowRect();
}

// Verify that the thumb paint function draws a round rectangle.
// Generally, NativeThemeFluent::Paint* functions are covered by
// Blink's web tests; but in web tests we render the thumbs as squares
// instead of pill-shaped. This test ensures we don't lose coverage on the
// PaintOp called to draw the thumb.
TEST_F(NativeThemeFluentTest, PaintThumbRoundedCorners) {
  cc::RecordPaintCanvas canvas;
  ColorProvider color_provider;
  constexpr gfx::Rect kRect(15, 100);
  // `is_web_test` is `false` by default.
  const NativeTheme::ScrollbarThumbExtraParams extra_params;
  theme_.PaintScrollbarThumb(
      &canvas, &color_provider,
      /*part=*/NativeTheme::kScrollbarVerticalThumb,
      /*state=*/NativeTheme::kNormal, kRect, extra_params,
      /*color_scheme=*/NativeTheme::ColorScheme::kDefault);
  EXPECT_EQ(canvas.TotalOpCount(), 1u);
  EXPECT_EQ(canvas.ReleaseAsRecord().GetFirstOp().GetType(),
            cc::PaintOpType::kDrawRRect);
}

// Verify that GetThumbColor returns the correct color given the scrollbar state
// and extra params.
TEST_F(NativeThemeFluentTest, GetThumbColor) {
  const std::unique_ptr<ui::ColorProvider> color_provider =
      CreateDefaultColorProviderForBlink(/*dark_mode=*/false);
  NativeTheme::ScrollbarThumbExtraParams extra_params;
  const auto to_skcolor = [&](auto id) {
    return SkColor4f::FromColor(color_provider->GetColor(id));
  };
  const auto scrollbar_color = [&](auto state) {
    return theme_.GetScrollbarThumbColor(*color_provider, state, extra_params);
  };

  const SkColor4f normal_thumb_color =
      to_skcolor(kColorWebNativeControlScrollbarThumb);
  const SkColor4f hovered_thumb_color =
      to_skcolor(kColorWebNativeControlScrollbarThumbHovered);
  const SkColor4f pressed_thumb_color =
      to_skcolor(kColorWebNativeControlScrollbarThumbPressed);
  const SkColor4f minimal_thumb_color =
      to_skcolor(kColorWebNativeControlScrollbarThumbOverlayMinimalMode);
  static constexpr SkColor css_color = SK_ColorRED;

  // When there are no extra params set, the colors should be the ones that
  // correspond to the ColorId.
  EXPECT_EQ(normal_thumb_color, scrollbar_color(NativeTheme::kNormal));
  EXPECT_EQ(hovered_thumb_color, scrollbar_color(NativeTheme::kHovered));
  EXPECT_EQ(pressed_thumb_color, scrollbar_color(NativeTheme::kPressed));

  // When the thumb is being painted in minimal mode, the normal state should
  // return the minimal mode's transparent color while the other states remain
  // unaffected.
  extra_params.is_thumb_minimal_mode = true;
  EXPECT_EQ(minimal_thumb_color, scrollbar_color(NativeTheme::kNormal));
  EXPECT_EQ(hovered_thumb_color, scrollbar_color(NativeTheme::kHovered));
  EXPECT_EQ(pressed_thumb_color, scrollbar_color(NativeTheme::kPressed));

  // When there is a css color set in the extra params, the state is overridden
  // and the scrollbars always return this color.
  extra_params.thumb_color = css_color;
  EXPECT_EQ(to_skcolor(css_color), scrollbar_color(NativeTheme::kNormal));
  EXPECT_EQ(to_skcolor(css_color), scrollbar_color(NativeTheme::kHovered));
  EXPECT_EQ(to_skcolor(css_color), scrollbar_color(NativeTheme::kPressed));
}

INSTANTIATE_TEST_SUITE_P(All,
                         NativeThemeFluentTest,
                         ::testing::Values(1.f, 1.25f, 1.5f, 1.75f, 2.f));

}  // namespace ui
