// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_fluent.h"

#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
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

INSTANTIATE_TEST_SUITE_P(All,
                         NativeThemeFluentTest,
                         ::testing::Values(1.f, 1.25f, 1.5f, 1.75f, 2.f));

}  // namespace ui
