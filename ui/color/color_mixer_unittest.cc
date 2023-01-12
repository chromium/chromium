// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixer.h"

#include <set>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_test_ids.h"
#include "ui/gfx/color_palette.h"

namespace {

ui::ColorMixer::MixerGetter PassThrough(const ui::ColorMixer* mixer) {
  return base::BindRepeating([](const ui::ColorMixer* mixer) { return mixer; },
                             mixer);
}

}  // namespace

namespace ui {
namespace {

// Tests that the recipe returned by operator[] is respected by the mixer.
TEST(ColorMixerTest, AccessOperator) {
  ColorMixer mixer;
  mixer[kColorTest0] = {SK_ColorGREEN};
  EXPECT_EQ(SK_ColorGREEN, mixer.GetResultColor(kColorTest0));
}

// Tests that GetInputColor() returns a placeholder color if the mixer has no
// recipes.
TEST(ColorMixerTest, GetInputColorNoInput) {
  EXPECT_EQ(gfx::kPlaceholderColor, ColorMixer().GetInputColor(kColorTest0));
}

// Tests that GetInputColor() on a given mixer ignores any recipe that mixer has
// for that color.
TEST(ColorMixerTest, GetInputColorIgnoresRecipe) {
  ColorMixer mixer;
  mixer[kColorTest0] = GetColorWithMaxContrast(FromTransformInput());
  EXPECT_EQ(gfx::kPlaceholderColor, mixer.GetInputColor(kColorTest0));
}

// Tests that if GetInputColor() needs to reference the output of a previous
// mixer, it will reference the result color (i.e. after applying any recipe)
// rather than referencing that mixer's input color.
TEST(ColorMixerTest, GetInputColorRespectsRecipePreviousMixer) {
  ColorMixer mixer0;
  mixer0[kColorTest0] = GetColorWithMaxContrast(FromTransformInput());
  ColorMixer mixer1(PassThrough(&mixer0));
  mixer1[kColorTest1] = {SK_ColorRED};
  EXPECT_EQ(color_utils::GetColorWithMaxContrast(gfx::kPlaceholderColor),
            mixer1.GetInputColor(kColorTest0));
}

// Tests that GetResultColor() returns a placeholder color when there are no
// recipes.
TEST(ColorMixerTest, GetResultColorNoInput) {
  EXPECT_EQ(gfx::kPlaceholderColor, ColorMixer().GetResultColor(kColorTest0));
}

// Tests that GetResultColor() does not require an input set to provide an
// initial value for its requested color.
TEST(ColorMixerTest, GetResultColorNoSet) {
  ColorMixer mixer;
  mixer[kColorTest0] = {SK_ColorGREEN};
  mixer[kColorTest1] = GetColorWithMaxContrast(FromTransformInput());
  EXPECT_EQ(SK_ColorGREEN, mixer.GetResultColor(kColorTest0));
  EXPECT_NE(gfx::kPlaceholderColor, mixer.GetResultColor(kColorTest1));
}

// Tests that GetResultColor() can reference other result colors, each of which
// will have its recipe applied.
TEST(ColorMixerTest, GetResultColorChained) {
  ColorMixer mixer;
  mixer[kColorTest0] = {gfx::kGoogleBlue050};
  mixer[kColorTest1] =
      BlendTowardMaxContrast(GetColorWithMaxContrast(SK_ColorWHITE), 0x29);
  mixer[kColorTest2] =
      BlendForMinContrast(gfx::kGoogleBlue500, kColorTest1, kColorTest0);
  EXPECT_EQ(SkColorSetRGB(0x89, 0xB3, 0xF8), mixer.GetResultColor(kColorTest2));
}

// Tests that GetResultColor() will use an input getter, if specified, to source
// input colors for recipes.
TEST(ColorMixerTest, GetResultColorWithInputGetter) {
  const ColorMixer* front_mixer;
  const auto getter = base::BindRepeating(
      [](const ColorMixer** mixer) { return *mixer; }, &front_mixer);
  ColorMixer mixer0(PassThrough(nullptr), getter);
  ColorMixer mixer1(PassThrough(&mixer0), getter);
  front_mixer = &mixer1;
  mixer0[kColorTest0] = {SK_ColorWHITE};
  mixer0[kColorTest1] = GetColorWithMaxContrast(kColorTest0);
  mixer1[kColorTest0] = {SK_ColorBLACK};
  const SkColor output = mixer0.GetResultColor(kColorTest1);
  EXPECT_EQ(output, mixer1.GetResultColor(kColorTest1));
  EXPECT_FALSE(color_utils::IsDark(output));
}

// Tests that GetDefinedColorIds() returns the ColorIds expected.
TEST(ColorMixerTest, GetDefinedColorIdsReturnsExpectedColorIds) {
  ColorMixer mixer;
  mixer[kColorTest0] = {gfx::kGoogleBlue050};
  mixer[kColorTest1] = BlendTowardMaxContrast(
      GetColorWithMaxContrast(FromTransformInput()), 0x29);
  mixer[kColorTest2] =
      BlendForMinContrast(gfx::kGoogleBlue500, kColorTest1, kColorTest0);
  EXPECT_EQ(std::set<ColorId>({kColorTest0, kColorTest1, kColorTest2}),
            mixer.GetDefinedColorIds());
}

}  // namespace
}  // namespace ui
