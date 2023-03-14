// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_transform.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_test_ids.h"
#include "ui/gfx/color_palette.h"

namespace ui {
namespace {

// Tests initialization with an SkColor value.
TEST(ColorRecipeTest, TestColorRecipeInitialization) {
  const auto verify_initialization = [&](SkColor color) {
    ColorTransform transform = color;
    EXPECT_EQ(color, transform.Run(SK_ColorBLACK, ColorMixer()));
  };
  verify_initialization(SK_ColorBLUE);
  verify_initialization(SK_ColorRED);
  verify_initialization(SK_ColorGREEN);
}

// Tests that AlphaBlend() produces a transform that blends its inputs.
TEST(ColorTransformTest, AlphaBlend) {
  const auto blend = [](SkAlpha alpha) {
    const ColorTransform transform =
        AlphaBlend(SK_ColorWHITE, SK_ColorBLACK, alpha);
    return transform.Run(gfx::kPlaceholderColor, ColorMixer());
  };
  EXPECT_EQ(SK_ColorBLACK, blend(SK_AlphaTRANSPARENT));
  EXPECT_EQ(SK_ColorWHITE, blend(SK_AlphaOPAQUE));
  EXPECT_EQ(SK_ColorGRAY, blend(SkColorGetR(SK_ColorGRAY)));
}

// Tests that BlendForMinContrast(), with the default args, produces a transform
// that blends its foreground color to produce readable contrast against its
// background color.
TEST(ColorTransformTest, BlendForMinContrast) {
  const ColorTransform transform =
      BlendForMinContrast(FromTransformInput(), kColorTest0);
  constexpr SkColor kBackground = SK_ColorWHITE;
  ColorMixer mixer;
  mixer[kColorTest0] = {kBackground};
  const auto verify_contrast = [&](SkColor input) {
    EXPECT_GE(
        color_utils::GetContrastRatio(transform.Run(input, mixer), kBackground),
        color_utils::kMinimumReadableContrastRatio);
  };
  verify_contrast(SK_ColorBLACK);
  verify_contrast(SK_ColorWHITE);
  verify_contrast(SK_ColorRED);
}

// Tests that BlendForMinContrast() supports optional args, which can be used to
// blend toward a specific foreground color and with a specific minimum contrast
// ratio.
TEST(ColorTransformTest, BlendForMinContrastOptionalArgs) {
  constexpr float kMinContrast = 6.0f;
  const ColorTransform transform = BlendForMinContrast(
      FromTransformInput(), kColorTest0, kColorTest1, kMinContrast);
  constexpr SkColor kBackground = SK_ColorWHITE;
  ColorMixer mixer;
  mixer[kColorTest0] = {kBackground};
  mixer[kColorTest1] = {gfx::kGoogleBlue900};
  const auto verify_contrast = [&](SkColor input) {
    EXPECT_GE(
        color_utils::GetContrastRatio(transform.Run(input, mixer), kBackground),
        kMinContrast);
  };
  verify_contrast(SK_ColorBLACK);
  verify_contrast(SK_ColorWHITE);
  verify_contrast(gfx::kGoogleBlue500);
}

// Tests that BlendForMinContrastWithSelf() produces a transform that blends its
// input color towards the color with max contrast.
TEST(ColorTransformTest, BlendForMinContrastWithSelf) {
  constexpr float kContrastRatio = 2;
  const ColorTransform transform =
      BlendForMinContrastWithSelf(FromTransformInput(), kContrastRatio);
  const auto verify_blend = [&](SkColor input) {
    const SkColor target = color_utils::GetColorWithMaxContrast(input);
    EXPECT_LT(color_utils::GetContrastRatio(transform.Run(input, ColorMixer()),
                                            target),
              color_utils::GetContrastRatio(input, target));
  };
  verify_blend(SK_ColorBLACK);
  verify_blend(SK_ColorWHITE);
  verify_blend(SK_ColorRED);
}

// Tests that BlendTowardMaxContrast() produces a transform that blends its
// input color towards the color with max contrast.
TEST(ColorTransformTest, BlendTowardMaxContrast) {
  constexpr SkAlpha kAlpha = 0x20;
  const ColorTransform transform =
      BlendTowardMaxContrast(FromTransformInput(), kAlpha);
  const auto verify_blend = [&](SkColor input) {
    const SkColor target = color_utils::GetColorWithMaxContrast(input);
    EXPECT_LT(color_utils::GetContrastRatio(transform.Run(input, ColorMixer()),
                                            target),
              color_utils::GetContrastRatio(input, target));
  };
  verify_blend(SK_ColorBLACK);
  verify_blend(SK_ColorWHITE);
  verify_blend(SK_ColorRED);
}

// Tests that ContrastInvert() produces a transform that outputs a color with at
// least as much contrast, but against the opposite endpoint.
TEST(ColorTransformTest, ContrastInvert) {
  const ColorTransform transform = ContrastInvert(FromTransformInput());
  const auto verify_invert = [&](SkColor input) {
    const SkColor far_endpoint = color_utils::GetColorWithMaxContrast(input);
    const SkColor near_endpoint =
        color_utils::GetColorWithMaxContrast(far_endpoint);
    EXPECT_GE(color_utils::GetContrastRatio(transform.Run(input, ColorMixer()),
                                            near_endpoint),
              color_utils::GetContrastRatio(input, far_endpoint));
  };
  verify_invert(gfx::kGoogleGrey900);
  verify_invert(SK_ColorWHITE);
  verify_invert(SK_ColorRED);
  verify_invert(gfx::kGoogleBlue500);
}

// Tests that DeriveDefaultIconColor() produces a transform that changes its
// input color.
TEST(ColorTransformTest, DeriveDefaultIconColor) {
  const ColorTransform transform = DeriveDefaultIconColor(FromTransformInput());
  const auto verify_derive = [&](SkColor input) {
    EXPECT_NE(input, transform.Run(input, ColorMixer()));
  };
  verify_derive(SK_ColorBLACK);
  verify_derive(SK_ColorWHITE);
  verify_derive(SK_ColorRED);
}

// Tests that initializing a transform from a color produces a transform that
// ignores the input color and always outputs a specified SkColor.
TEST(ColorTransformTest, FromColor) {
  constexpr SkColor kOutput = SK_ColorGREEN;
  const ColorTransform transform = kOutput;
  const auto verify_color = [&](SkColor input) {
    EXPECT_EQ(kOutput, transform.Run(input, ColorMixer()));
  };
  verify_color(SK_ColorBLACK);
  verify_color(SK_ColorWHITE);
  verify_color(SK_ColorRED);
}

// Tests that a transform created from a ColorId produces a transform that
// ignores the input color and always outputs a specified result color.
TEST(ColorTransformTest, FromColorId) {
  const ColorTransform transform = {kColorTest0};
  constexpr SkColor kTest1Color = SK_ColorRED;
  ColorMixer mixer;
  mixer[kColorTest0] = {kColorTest1};
  mixer[kColorTest1] = {kTest1Color};
  const auto verify_color = [&](SkColor input) {
    EXPECT_EQ(kTest1Color, transform.Run(input, mixer));
  };
  verify_color(SK_ColorBLACK);
  verify_color(SK_ColorWHITE);
  verify_color(SK_ColorRED);
}  // namespace

// Tests that FromTransformInput() returns its input color unmodified.
TEST(ColorTransformTest, FromTransformInput) {
  const ColorTransform transform = FromTransformInput();
  const auto verify_color = [&](SkColor input) {
    EXPECT_EQ(input, transform.Run(input, ColorMixer()));
  };
  verify_color(SK_ColorBLACK);
  verify_color(SK_ColorWHITE);
  verify_color(SK_ColorRED);
}  // namespace

// Tests that GetColorWithMaxContrast() produces a transform that changes white
// to the darkest color.
TEST(ColorTransformTest, GetColorWithMaxContrast) {
  const ColorTransform transform =
      GetColorWithMaxContrast(FromTransformInput());
  constexpr SkColor kNewDarkestColor = gfx::kGoogleGrey500;
  const SkColor default_darkest_color =
      color_utils::SetDarkestColorForTesting(kNewDarkestColor);
  constexpr SkColor kLightestColor = SK_ColorWHITE;
  EXPECT_EQ(kNewDarkestColor, transform.Run(kLightestColor, ColorMixer()));
  color_utils::SetDarkestColorForTesting(default_darkest_color);
  EXPECT_EQ(default_darkest_color, transform.Run(kLightestColor, ColorMixer()));
}

// Tests that GetResultingPaintColor() produces a transform that composites
// opaquely.
TEST(ColorTransformTest, GetResultingPaintColor) {
  const ColorTransform transform =
      GetResultingPaintColor(FromTransformInput(), kColorTest0);
  constexpr SkColor kBackground = SK_ColorWHITE;
  ColorMixer mixer;
  mixer[kColorTest0] = {kBackground};
  EXPECT_EQ(SK_ColorBLACK, transform.Run(SK_ColorBLACK, mixer));
  EXPECT_EQ(kBackground, transform.Run(SK_ColorTRANSPARENT, mixer));
  EXPECT_EQ(color_utils::AlphaBlend(SK_ColorBLACK, kBackground, SkAlpha{0x80}),
            transform.Run(SkColorSetA(SK_ColorBLACK, 0x80), mixer));
}

// Tests that SelectBasedOnDarkInput() produces a transform that toggles between
// inputs based on whether the input color is dark.
TEST(ColorTransformTest, SelectBasedOnDarkInput) {
  constexpr SkColor kDarkOutput = SK_ColorGREEN;
  constexpr SkColor kLightOutput = SK_ColorRED;
  const ColorTransform transform =
      SelectBasedOnDarkInput(FromTransformInput(), kDarkOutput, kLightOutput);
  EXPECT_EQ(kDarkOutput, transform.Run(SK_ColorBLACK, ColorMixer()));
  EXPECT_EQ(kLightOutput, transform.Run(SK_ColorWHITE, ColorMixer()));
  EXPECT_EQ(kDarkOutput, transform.Run(SK_ColorBLUE, ColorMixer()));
  EXPECT_EQ(kLightOutput, transform.Run(SK_ColorRED, ColorMixer()));
}

// Tests that SetAlpha() produces a transform that sets its input's alpha.
TEST(ColorTransformTest, SetAlpha) {
  constexpr SkAlpha kAlpha = 0x20;
  const ColorTransform transform = SetAlpha(FromTransformInput(), kAlpha);
  for (auto color : {SK_ColorBLACK, SK_ColorRED, SK_ColorTRANSPARENT})
    EXPECT_EQ(SkColorSetA(color, kAlpha), transform.Run(color, ColorMixer()));
}

// Tests that PickGoogleColor() produces a transform that picks a Google color
// with appropriate contrast against the specified background.
TEST(ColorTransformTest, PickGoogleColor) {
  constexpr SkColor kBackground = gfx::kGoogleGrey600;
  constexpr float kMinContrast = 2.1f;
  const ColorTransform transform =
      PickGoogleColor(FromTransformInput(), kBackground, kMinContrast);
  EXPECT_EQ(gfx::kGoogleRed900, transform.Run(SK_ColorRED, ColorMixer()));
  EXPECT_EQ(gfx::kGoogleGreen100, transform.Run(SK_ColorGREEN, ColorMixer()));
  EXPECT_EQ(gfx::kGoogleBlue900, transform.Run(SK_ColorBLUE, ColorMixer()));
}

// Tests that PickGoogleColorTwoBackgrounds() produces a transform that picks a
// Google color with appropriate contrast against both specified backgrounds.
TEST(ColorTransformTest, PickGoogleColorTwoBackgrounds) {
  constexpr SkColor kBackgroundA = gfx::kGoogleGrey800;
  constexpr SkColor kBackgroundB = gfx::kGoogleGrey400;
  constexpr float kMinContrast = 1.5f;
  const ColorTransform transform = PickGoogleColorTwoBackgrounds(
      FromTransformInput(), kBackgroundA, kBackgroundB, kMinContrast);
  EXPECT_EQ(gfx::kGoogleRed500, transform.Run(SK_ColorRED, ColorMixer()));
  EXPECT_EQ(gfx::kGoogleGreen050, transform.Run(SK_ColorGREEN, ColorMixer()));
  EXPECT_EQ(gfx::kGoogleBlue800, transform.Run(SK_ColorBLUE, ColorMixer()));
}

// Tests that HSLShift() produces a transform that applies the given HSL shift
// to the given input color.
TEST(ColorTransformTest, HSLShift) {
  constexpr color_utils::HSL kHsl = {0.2, 0.3, 0.4};
  const ColorTransform transform = HSLShift(FromTransformInput(), kHsl);
  const auto verify_color = [&](SkColor input) {
    EXPECT_EQ(color_utils::HSLShift(input, kHsl),
              transform.Run(input, ColorMixer()));
  };
  verify_color(SK_ColorBLACK);
  verify_color(gfx::kGoogleGrey600);
  verify_color(SK_ColorWHITE);
}

}  // namespace
}  // namespace ui
