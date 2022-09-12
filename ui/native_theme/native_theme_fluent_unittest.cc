// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_fluent.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme_constants_fluent.h"

namespace ui {

class NativeThemeFluentTest : public ::testing::Test,
                              public ::testing::WithParamInterface<float> {
 protected:
  int ArrowRectLength() const {
    const int arrow_rect_length =
        base::ClampFloor(kFluentScrollbarArrowRectLength * ScaleFromDIP());
    return (TrackThickness() - arrow_rect_length) % 2 == 0
               ? arrow_rect_length
               : arrow_rect_length + 1;
  }

  // Returns an arrow rect x() coordinate for vertical arrows.
  int ArrowRectX() const {
    EXPECT_EQ((TrackThickness() - ArrowRectLength()) % 2, 0);
    return base::ClampFloor((TrackThickness() - ArrowRectLength()) / 2.0f);
  }

  // Returns an arrow rect y() coordinate for vertical arrows.
  int ArrowRectY() const {
    const float dsf = ScaleFromDIP();
    if (dsf == 1.f)
      return 4;
    if (dsf == 1.25f)
      return 4;
    if (dsf == 1.5f)
      return 5;
    if (dsf == 1.75f)
      return 6;
    if (dsf == 2.f)
      return 8;

    NOTREACHED();
    return 0;
  }

  int ButtonLength() const {
    return base::ClampFloor(kFluentScrollbarButtonSideLength * ScaleFromDIP());
  }

  int TrackThickness() const {
    return base::ClampFloor(kFluentScrollbarThickness * ScaleFromDIP());
  }

  float ScaleFromDIP() const { return GetParam(); }
};

TEST_P(NativeThemeFluentTest, VerticalArrowRectDefault) {
  const gfx::Rect button_rect(0, 0, TrackThickness(), ButtonLength());
  const NativeThemeFluent theme(false);

  EXPECT_EQ(theme.GetArrowRect(button_rect, NativeTheme::kScrollbarUpArrow,
                               NativeTheme::kNormal),
            gfx::Rect(ArrowRectX(), ArrowRectY(), ArrowRectLength(),
                      ArrowRectLength()));
}

TEST_P(NativeThemeFluentTest, HorizontalArrowRectDefault) {
  const gfx::Rect button_rect(0, 0, ButtonLength(), TrackThickness());
  const NativeThemeFluent theme(false);

  EXPECT_EQ(theme.GetArrowRect(button_rect, NativeTheme::kScrollbarLeftArrow,
                               NativeTheme::kNormal),
            gfx::Rect(ArrowRectY(), ArrowRectX(), ArrowRectLength(),
                      ArrowRectLength()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         NativeThemeFluentTest,
                         ::testing::Values(1.f, 1.25f, 1.5f, 1.75f, 2.f));

}  // namespace ui
