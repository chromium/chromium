// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_base.h"

#include <array>
#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

class NativeThemeBaseTest : public NativeThemeBase, public testing::Test {
 public:
  SkColor GetContrastingColorForScrollbarPart(
      SkColor fg,
      std::optional<SkColor> bg = std::nullopt) {
    return NativeThemeBase::GetContrastingColorForScrollbarPart(
               fg, bg, NativeTheme::State::kHovered)
        .value();
  }
  float GetScrollbarPartContrastRatio() {
    return NativeThemeBase::GetScrollbarPartContrastRatioForState(
        NativeTheme::State::kHovered);
  }
};

// Check that `GetContrastingColorForScrollbarPart()` doesn't modify
// fully-transparent colors.
TEST_F(NativeThemeBaseTest, GetContrastingColorTransparent) {
  static constexpr auto kTransparentColor =
      SkColorSetARGB(0x00, 0xBA, 0x74, 0x74);
  EXPECT_EQ(kTransparentColor,
            GetContrastingColorForScrollbarPart(kTransparentColor));
}

// Tests that the `GetContrastingColorForScrollbarPart` can adapt to a whole
// range of luminosity for the colors to modify.
TEST_F(NativeThemeBaseTest, GetContrastingColorForScrollbarPart) {
  for (unsigned i = 0; i < 255; i++) {
    const SkColor color = SkColorSetRGB(i, i, i);
    const float luminance = color_utils::GetRelativeLuminance(color);
    const float adjusted_luminance = color_utils::GetRelativeLuminance(
        GetContrastingColorForScrollbarPart(color));
    if (color_utils::IsDark(color)) {
      EXPECT_GT(adjusted_luminance, luminance);
    } else {
      EXPECT_LT(adjusted_luminance, luminance);
    }
  }
}

// Tests that the returned color never loses contrast against the background
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
