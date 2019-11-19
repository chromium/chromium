// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace color_utils {

TEST(ColorUtils, SkColorToHSLRed) {
  HSL hsl = {0, 0, 0};
  SkColorToHSL(SK_ColorRED, &hsl);
  EXPECT_DOUBLE_EQ(hsl.h, 0);
  EXPECT_DOUBLE_EQ(hsl.s, 1);
  EXPECT_DOUBLE_EQ(hsl.l, 0.5);
}

TEST(ColorUtils, SkColorToHSLGrey) {
  HSL hsl = {0, 0, 0};
  SkColorToHSL(SkColorSetARGB(255, 128, 128, 128), &hsl);
  EXPECT_DOUBLE_EQ(hsl.h, 0);
  EXPECT_DOUBLE_EQ(hsl.s, 0);
  EXPECT_EQ(static_cast<int>(hsl.l * 100),
            static_cast<int>(0.5 * 100));  // Accurate to two decimal places.
}

TEST(ColorUtils, HSLToSkColorWithAlpha) {
  SkColor red = SkColorSetARGB(128, 255, 0, 0);
  HSL hsl = {0, 1, 0.5};
  SkColor result = HSLToSkColor(hsl, 128);
  EXPECT_EQ(SkColorGetA(red), SkColorGetA(result));
  EXPECT_EQ(SkColorGetR(red), SkColorGetR(result));
  EXPECT_EQ(SkColorGetG(red), SkColorGetG(result));
  EXPECT_EQ(SkColorGetB(red), SkColorGetB(result));
}

TEST(ColorUtils, RGBtoHSLRoundTrip) {
  // Just spot check values near the edges.
  for (int r = 0; r < 10; ++r) {
    for (int g = 0; g < 10; ++g) {
      for (int b = 0; b < 10; ++b) {
        SkColor rgb = SkColorSetARGB(255, r, g, b);
        HSL hsl = {0, 0, 0};
        SkColorToHSL(rgb, &hsl);
        SkColor out = HSLToSkColor(hsl, 255);
        EXPECT_EQ(SkColorGetR(out), SkColorGetR(rgb));
        EXPECT_EQ(SkColorGetG(out), SkColorGetG(rgb));
        EXPECT_EQ(SkColorGetB(out), SkColorGetB(rgb));
      }
    }
  }
  for (int r = 240; r < 256; ++r) {
    for (int g = 240; g < 256; ++g) {
      for (int b = 240; b < 256; ++b) {
        SkColor rgb = SkColorSetARGB(255, r, g, b);
        HSL hsl = {0, 0, 0};
        SkColorToHSL(rgb, &hsl);
        SkColor out = HSLToSkColor(hsl, 255);
        EXPECT_EQ(SkColorGetR(out), SkColorGetR(rgb));
        EXPECT_EQ(SkColorGetG(out), SkColorGetG(rgb));
        EXPECT_EQ(SkColorGetB(out), SkColorGetB(rgb));
      }
    }
  }
}

TEST(ColorUtils, IsWithinHSLRange) {
  HSL hsl = {0.3, 0.4, 0.5};
  HSL lower = {0.2, 0.3, 0.4};
  HSL upper = {0.4, 0.5, 0.6};
  EXPECT_TRUE(IsWithinHSLRange(hsl, lower, upper));
  // Bounds are inclusive.
  hsl.h = 0.2;
  EXPECT_TRUE(IsWithinHSLRange(hsl, lower, upper));
  hsl.h = 0.4;
  EXPECT_TRUE(IsWithinHSLRange(hsl, lower, upper));
  hsl.s = 0.3;
  EXPECT_TRUE(IsWithinHSLRange(hsl, lower, upper));
  hsl.s = 0.5;
  EXPECT_TRUE(IsWithinHSLRange(hsl, lower, upper));
  hsl.l = 0.4;
  EXPECT_TRUE(IsWithinHSLRange(hsl, lower, upper));
  hsl.l = 0.6;
  EXPECT_TRUE(IsWithinHSLRange(hsl, lower, upper));
}

TEST(ColorUtils, IsWithinHSLRangeHueWrapAround) {
  HSL hsl = {0.3, 0.4, 0.5};
  HSL lower = {0.8, -1, -1};
  HSL upper = {1.2, -1, -1};
  hsl.h = 0.1;
  EXPECT_TRUE(IsWithinHSLRange(hsl, lower, upper));
  hsl.h = 0.9;
  EXPECT_TRUE(IsWithinHSLRange(hsl, lower, upper));
  hsl.h = 0.3;
  EXPECT_FALSE(IsWithinHSLRange(hsl, lower, upper));
}

TEST(ColorUtils, IsHSLShiftMeaningful) {
  HSL noop_all_neg_one{-1.0, -1.0, -1.0};
  HSL noop_s_point_five{-1.0, 0.5, -1.0};
  HSL noop_l_point_five{-1.0, -1.0, 0.5};

  HSL only_h{0.1, -1.0, -1.0};
  HSL only_s{-1.0, 0.1, -1.0};
  HSL only_l{-1.0, -1.0, 0.1};
  HSL only_hs{0.1, 0.1, -1.0};
  HSL only_hl{0.1, -1.0, 0.1};
  HSL only_sl{-1.0, 0.1, 0.1};
  HSL all_set{0.1, 0.2, 0.3};

  EXPECT_FALSE(IsHSLShiftMeaningful(noop_all_neg_one));
  EXPECT_FALSE(IsHSLShiftMeaningful(noop_s_point_five));
  EXPECT_FALSE(IsHSLShiftMeaningful(noop_l_point_five));

  EXPECT_TRUE(IsHSLShiftMeaningful(only_h));
  EXPECT_TRUE(IsHSLShiftMeaningful(only_s));
  EXPECT_TRUE(IsHSLShiftMeaningful(only_l));
  EXPECT_TRUE(IsHSLShiftMeaningful(only_hs));
  EXPECT_TRUE(IsHSLShiftMeaningful(only_hl));
  EXPECT_TRUE(IsHSLShiftMeaningful(only_sl));
  EXPECT_TRUE(IsHSLShiftMeaningful(all_set));
}

TEST(ColorUtils, ColorToHSLRegisterSpill) {
  // In a opt build on Linux, this was causing a register spill on my laptop
  // (Pentium M) when converting from SkColor to HSL.
  SkColor input = SkColorSetARGB(255, 206, 154, 89);
  HSL hsl = {-1, -1, -1};
  SkColor result = HSLShift(input, hsl);
  // |result| should be the same as |input| since we passed in a value meaning
  // no color shift.
  EXPECT_EQ(SkColorGetA(input), SkColorGetA(result));
  EXPECT_EQ(SkColorGetR(input), SkColorGetR(result));
  EXPECT_EQ(SkColorGetG(input), SkColorGetG(result));
  EXPECT_EQ(SkColorGetB(input), SkColorGetB(result));
}

TEST(ColorUtils, CalculateBoringScore_Empty) {
  SkBitmap bitmap;
  EXPECT_DOUBLE_EQ(1.0, CalculateBoringScore(bitmap));
}

TEST(ColorUtils, CalculateBoringScore_SingleColor) {
  const gfx::Size kSize(20, 10);
  gfx::Canvas canvas(kSize, 1.0f, true);
  // Fill all pixels in black.
  canvas.FillRect(gfx::Rect(kSize), SK_ColorBLACK);

  SkBitmap bitmap = canvas.GetBitmap();
  // The thumbnail should deserve the highest boring score.
  EXPECT_DOUBLE_EQ(1.0, CalculateBoringScore(bitmap));
}

TEST(ColorUtils, CalculateBoringScore_TwoColors) {
  const gfx::Size kSize(20, 10);

  gfx::Canvas canvas(kSize, 1.0f, true);
  // Fill all pixels in black.
  canvas.FillRect(gfx::Rect(kSize), SK_ColorBLACK);
  // Fill the left half pixels in white.
  canvas.FillRect(gfx::Rect(0, 0, kSize.width() / 2, kSize.height()),
                  SK_ColorWHITE);

  SkBitmap bitmap = canvas.GetBitmap();
  ASSERT_EQ(kSize.width(), bitmap.width());
  ASSERT_EQ(kSize.height(), bitmap.height());
  // The thumbnail should be less boring because two colors are used.
  EXPECT_DOUBLE_EQ(0.5, CalculateBoringScore(bitmap));
}

TEST(ColorUtils, AlphaBlend) {
  SkColor fore = SkColorSetARGB(255, 200, 200, 200);
  SkColor back = SkColorSetARGB(255, 100, 100, 100);

  EXPECT_TRUE(AlphaBlend(fore, back, 1.0f) == fore);
  EXPECT_TRUE(AlphaBlend(fore, back, 0.0f) == back);

  // One is fully transparent, result is partially transparent.
  back = SkColorSetA(back, 0);
  EXPECT_EQ(136U, SkColorGetA(AlphaBlend(fore, back, SkAlpha{136})));

  // Both are fully transparent, result is fully transparent.
  fore = SkColorSetA(fore, 0);
  EXPECT_EQ(0U, SkColorGetA(AlphaBlend(fore, back, 1.0f)));
}

TEST(ColorUtils, SkColorToRgbaString) {
  SkColor color = SkColorSetARGB(153, 100, 150, 200);
  std::string color_string = SkColorToRgbaString(color);
  EXPECT_EQ(color_string, "rgba(100,150,200,0.6)");
}

TEST(ColorUtils, SkColorToRgbString) {
  SkColor color = SkColorSetARGB(200, 50, 100, 150);
  std::string color_string = SkColorToRgbString(color);
  EXPECT_EQ(color_string, "50,100,150");
}

TEST(ColorUtils, IsDarkDarkestColorChange) {
  ASSERT_FALSE(IsDark(SK_ColorLTGRAY));
  const SkColor old_darkest_color = SetDarkestColorForTesting(SK_ColorLTGRAY);
  EXPECT_TRUE(IsDark(SK_ColorLTGRAY));

  SetDarkestColorForTesting(old_darkest_color);
}

TEST(ColorUtils, MidpointLuminanceMatches) {
  const SkColor old_darkest_color = SetDarkestColorForTesting(SK_ColorBLACK);
  float darkest, midpoint, lightest;
  std::tie(darkest, midpoint, lightest) = GetLuminancesForTesting();
  EXPECT_FLOAT_EQ(GetContrastRatio(darkest, midpoint),
                  GetContrastRatio(midpoint, lightest));

  SetDarkestColorForTesting(old_darkest_color);
  std::tie(darkest, midpoint, lightest) = GetLuminancesForTesting();
  EXPECT_FLOAT_EQ(GetContrastRatio(darkest, midpoint),
                  GetContrastRatio(midpoint, lightest));
}

TEST(ColorUtils, GetColorWithMaxContrast) {
  const SkColor old_darkest_color = SetDarkestColorForTesting(SK_ColorBLACK);
  EXPECT_EQ(SK_ColorWHITE, GetColorWithMaxContrast(SK_ColorBLACK));
  EXPECT_EQ(SK_ColorWHITE,
            GetColorWithMaxContrast(SkColorSetRGB(0x75, 0x75, 0x75)));
  EXPECT_EQ(SK_ColorBLACK, GetColorWithMaxContrast(SK_ColorWHITE));
  EXPECT_EQ(SK_ColorBLACK,
            GetColorWithMaxContrast(SkColorSetRGB(0x76, 0x76, 0x76)));

  SetDarkestColorForTesting(old_darkest_color);
  EXPECT_EQ(old_darkest_color, GetColorWithMaxContrast(SK_ColorWHITE));
}

TEST(ColorUtils, BlendForMinContrast_ForegroundAlreadyMeetsMinimum) {
  const auto result = BlendForMinContrast(SK_ColorBLACK, SK_ColorWHITE);
  EXPECT_EQ(SK_AlphaTRANSPARENT, result.alpha);
  EXPECT_EQ(SK_ColorBLACK, result.color);
}

TEST(ColorUtils, BlendForMinContrast_BlendDarker) {
  const SkColor foreground = SkColorSetRGB(0xAA, 0xAA, 0xAA);
  const auto result = BlendForMinContrast(foreground, SK_ColorWHITE);
  EXPECT_NE(SK_AlphaTRANSPARENT, result.alpha);
  EXPECT_NE(foreground, result.color);
  EXPECT_GE(GetContrastRatio(result.color, SK_ColorWHITE),
            kMinimumReadableContrastRatio);
}

TEST(ColorUtils, BlendForMinContrast_BlendLighter) {
  const SkColor foreground = SkColorSetRGB(0x33, 0x33, 0x33);
  const auto result = BlendForMinContrast(foreground, SK_ColorBLACK);
  EXPECT_NE(SK_AlphaTRANSPARENT, result.alpha);
  EXPECT_NE(foreground, result.color);
  EXPECT_GE(GetContrastRatio(result.color, SK_ColorBLACK),
            kMinimumReadableContrastRatio);
}

TEST(ColorUtils, BlendForMinContrast_StopsAtDarkestColor) {
  const SkColor darkest_color = SkColorSetRGB(0x44, 0x44, 0x44);
  const SkColor old_darkest_color = SetDarkestColorForTesting(darkest_color);
  EXPECT_EQ(darkest_color, BlendForMinContrast(SkColorSetRGB(0x55, 0x55, 0x55),
                                               SkColorSetRGB(0xAA, 0xAA, 0xAA))
                               .color);

  SetDarkestColorForTesting(old_darkest_color);
}

TEST(ColorUtils, BlendForMinContrast_ComputesExpectedOpacities) {
  const SkColor source = SkColorSetRGB(0xDE, 0xE1, 0xE6);
  const SkColor target = SkColorSetRGB(0xFF, 0xFF, 0xFF);
  const SkColor base = source;
  SkAlpha alpha = BlendForMinContrast(source, base, target, 1.11f).alpha;
  EXPECT_NEAR(alpha / 255.0f, 0.4f, 0.03f);
  alpha = BlendForMinContrast(source, base, target, 1.19f).alpha;
  EXPECT_NEAR(alpha / 255.0f, 0.65f, 0.03f);
  alpha = BlendForMinContrast(source, base, target, 1.13728f).alpha;
  EXPECT_NEAR(alpha / 255.0f, 0.45f, 0.03f);
}

TEST(ColorUtils, BlendTowardMaxContrast_PreservesAlpha) {
  SkColor test_colors[] = {SK_ColorBLACK,      SK_ColorWHITE,
                           SK_ColorRED,        SK_ColorYELLOW,
                           SK_ColorMAGENTA,    gfx::kGoogleGreen500,
                           gfx::kGoogleRed050, gfx::kGoogleBlue800};
  SkAlpha test_alphas[] = {SK_AlphaTRANSPARENT, 0x0F,
                           gfx::kDisabledControlAlpha, 0xDD};
  SkAlpha blend_alpha = 0x7F;
  for (const SkColor color : test_colors) {
    SkColor opaque_result =
        color_utils::BlendTowardMaxContrast(color, blend_alpha);
    for (const SkAlpha alpha : test_alphas) {
      SkColor input = SkColorSetA(color, alpha);
      SkColor result = color_utils::BlendTowardMaxContrast(input, blend_alpha);
      // Alpha was preserved.
      EXPECT_EQ(SkColorGetA(result), alpha);
      // RGB channels unaffected by alpha of input.
      EXPECT_EQ(SkColorSetA(result, SK_AlphaOPAQUE), opaque_result);
    }
  }
}

TEST(ColorUtils, BlendForMinContrast_MatchesNaiveImplementation) {
  constexpr SkColor default_foreground = SkColorSetRGB(0xDE, 0xE1, 0xE6);
  constexpr SkColor high_contrast_foreground = SK_ColorWHITE;
  constexpr SkColor background = default_foreground;
  constexpr float kContrastRatio = 1.11f;
  const auto result = BlendForMinContrast(
      default_foreground, background, high_contrast_foreground, kContrastRatio);

  // Naive implementation is direct translation of function description.
  SkAlpha alpha = SK_AlphaTRANSPARENT;
  SkColor color = default_foreground;
  for (int i = SK_AlphaTRANSPARENT; i <= SK_AlphaOPAQUE; ++i) {
    alpha = SkAlpha{i};
    color = AlphaBlend(high_contrast_foreground, default_foreground, alpha);
    if (GetContrastRatio(color, background) >= kContrastRatio)
      break;
  }

  EXPECT_EQ(alpha, result.alpha);
  EXPECT_EQ(color, result.color);
}

}  // namespace color_utils
