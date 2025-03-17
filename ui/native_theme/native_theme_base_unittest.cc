// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_base.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/features/native_theme_features.h"

namespace ui {

class NativeThemeBaseTest : public NativeThemeBase, public testing::Test {
 public:
  SkColor GetContrastingPressedOrHoveredColor(
      SkColor fg,
      std::optional<SkColor> bg = std::nullopt) {
    return NativeThemeBase::GetContrastingPressedOrHoveredColor(
               fg, bg, /*state=*/NativeTheme::State::kHovered,
               /*part=*/Part::kScrollbarVerticalThumb)
        .value();
  }
  float GetBaseContrastRatio() {
    return NativeThemeBase::GetContrastRatioForState(
        /*state=*/NativeTheme::State::kHovered,
        /*part=*/Part::kScrollbarVerticalThumb);
  }
};

// Check that `GetContrastingPressedOrHoveredColor` doesn't modify fully
// transparent colors.
TEST_F(NativeThemeBaseTest, GetContrastingPressedOrHoveredTransparent) {
  const SkColor transparent_color = SkColorSetARGB(0x00, 0xBA, 0x74, 0x74);
  EXPECT_EQ(transparent_color,
            GetContrastingPressedOrHoveredColor(transparent_color));
}

// Tests that the `GetContrastingPressedOrHoveredColor` can adapt to a whole
// range of luminosity for the colors to modify.
TEST_F(NativeThemeBaseTest, GetContrastingPressedOrHoveredColor) {
  for (unsigned i = 0; i < 255; i++) {
    const SkColor color = SkColorSetRGB(i, i, i);
    const float luminance = color_utils::GetRelativeLuminance(color);
    const float adjusted_luminance = color_utils::GetRelativeLuminance(
        GetContrastingPressedOrHoveredColor(color));
    if (color_utils::IsDark(color)) {
      EXPECT_GT(adjusted_luminance, luminance);
    } else {
      EXPECT_LT(adjusted_luminance, luminance);
    }
  }
}

// Tests that the returned color never loses contrast against the background
// color.
TEST_F(NativeThemeBaseTest,
       GetContrastingPressedOrHoveredColorBackgroundContrast) {
  for (int c = 0; c < 24; c += 8) {
    // Test with Green, Blue and Red.
    const SkColor bg_color = SkColorSetA(SkColor(0x80 << (c)), SK_AlphaOPAQUE);
    for (U8CPU i = 0; i < 255; i++) {
      const SkColor adjusted_color = GetContrastingPressedOrHoveredColor(
          SkColorSetA(SkColor(i << (c)), SK_AlphaOPAQUE), bg_color);
      EXPECT_GT(color_utils::GetContrastRatio(bg_color, adjusted_color),
                GetBaseContrastRatio());
    }
  }
}

// Checks that grayscale colors never lose contrast with the background when it
// is different shades of gray.
TEST_F(NativeThemeBaseTest, GetContrastingPressedOrHoveredColorGrayScales) {
  static constexpr auto kColors =
      std::to_array({SK_ColorWHITE, SK_ColorLTGRAY, SK_ColorGRAY,
                     SK_ColorDKGRAY, SK_ColorBLACK});
  for (const auto foreground : kColors) {
    for (const auto background : kColors) {
      const float background_contrast_ratio = color_utils::GetContrastRatio(
          background,
          GetContrastingPressedOrHoveredColor(foreground, background));
      const float base_contrast_ratio = GetBaseContrastRatio();
      EXPECT_GE(background_contrast_ratio, base_contrast_ratio);
    }
  }
}

// Checks that transparent foreground colors don't lose contrast against the
// background when altered.
TEST_F(NativeThemeBaseTest, GetContrastingPressedOrHoveredTransparencies) {
  for (const auto background : {SK_ColorBLACK, SK_ColorWHITE}) {
    for (auto foreground : {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE}) {
      for (int i = 1; i < 4; i++) {
        foreground = SkColorSetA(foreground, SK_AlphaOPAQUE * i / 4);
        const float background_contrast_ratio = color_utils::GetContrastRatio(
            background,
            GetContrastingPressedOrHoveredColor(foreground, background));
        const float base_contrast_ratio = GetBaseContrastRatio();
        EXPECT_GE(background_contrast_ratio, base_contrast_ratio);
      }
    }
  }
}

// Tests that colors are not modified if the feature flag is disabled.
TEST_F(NativeThemeBaseTest, GetContrastingPressedOrHoveredDisableFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ::features::kModifyScrollbarCssColorOnHoverOrPress);
  EXPECT_EQ(GetContrastingPressedOrHoveredColor(SK_ColorRED, SK_ColorBLACK),
            SK_ColorRED);
}

}  // namespace ui
