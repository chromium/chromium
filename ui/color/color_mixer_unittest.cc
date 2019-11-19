// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_test_ids.h"
#include "ui/gfx/color_palette.h"

namespace ui {
namespace {

// Tests that AddSet() changes the result of various other functions.
TEST(ColorMixerTest, AddSet) {
  ColorMixer mixer;
  EXPECT_EQ(gfx::kPlaceholderColor, mixer.GetInputColor(kColorTest0));
  EXPECT_EQ(gfx::kPlaceholderColor,
            mixer.GetOriginalColorFromSet(kColorTest0, kColorSetTest0));
  EXPECT_EQ(gfx::kPlaceholderColor, mixer.GetResultColor(kColorTest0));

  mixer.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  EXPECT_EQ(SK_ColorGREEN, mixer.GetInputColor(kColorTest0));
  EXPECT_EQ(SK_ColorGREEN,
            mixer.GetOriginalColorFromSet(kColorTest0, kColorSetTest0));
  EXPECT_EQ(SK_ColorGREEN, mixer.GetResultColor(kColorTest0));
}

// Tests that the recipe returned by operator[] is respected by the mixer.
TEST(ColorMixerTest, AccessOperator) {
  ColorMixer mixer;
  mixer[kColorTest0] = ColorTransform(SK_ColorGREEN);
  EXPECT_EQ(SK_ColorGREEN, mixer.GetResultColor(kColorTest0));
}

// Tests that GetInputColor() returns a placeholder color if the mixer has no
// recipes.
TEST(ColorMixerTest, GetInputColorNoInput) {
  EXPECT_EQ(gfx::kPlaceholderColor, ColorMixer().GetInputColor(kColorTest0));
}

// Tests that GetInputColor() will find the color in a single input set and
// return a placeholder color for a color not in that set.
TEST(ColorMixerTest, GetInputColorOneSet) {
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  EXPECT_EQ(SK_ColorGREEN, mixer.GetInputColor(kColorTest0));
  EXPECT_EQ(gfx::kPlaceholderColor, mixer.GetInputColor(kColorTest1));
}

// Tests that GetInputColor() will find the colors in multiple non-overlapping
// input sets.
TEST(ColorMixerTest, GetInputColorTwoSetsNonOverlapping) {
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  mixer.AddSet({kColorSetTest1, {{kColorTest1, SK_ColorRED}}});
  EXPECT_EQ(SK_ColorGREEN, mixer.GetInputColor(kColorTest0));
  EXPECT_EQ(SK_ColorRED, mixer.GetInputColor(kColorTest1));
}

// Tests that when multiple input sets specify the same ID, the last set added
// wins.
TEST(ColorMixerTest, GetInputColorTwoSetsOverlapping) {
  ColorMixer mixer;
  // These sets are intentionally added out of numeric order to ensure the set
  // ID ordering is ignored.
  mixer.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorRED}}});
  mixer.AddSet({kColorSetTest2, {{kColorTest0, SK_ColorBLUE}}});
  mixer.AddSet({kColorSetTest1, {{kColorTest0, SK_ColorGREEN}}});
  EXPECT_EQ(SK_ColorGREEN, mixer.GetInputColor(kColorTest0));
}

// Tests that when a mixer does not have a requested color for GetInputColor(),
// it forwards the request to the previous mixer.
TEST(ColorMixerTest, GetInputColorPreviousMixer) {
  ColorMixer mixer0;
  mixer0.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  ColorMixer mixer1(&mixer0);
  mixer1.AddSet({kColorSetTest1, {{kColorTest1, SK_ColorRED}}});
  EXPECT_EQ(SK_ColorGREEN, mixer1.GetInputColor(kColorTest0));
}

// Tests that GetInputColor() on a given mixer ignores any recipe that mixer has
// for that color.
TEST(ColorMixerTest, GetInputColorIgnoresRecipe) {
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  mixer[kColorTest0] = GetColorWithMaxContrast(FromTransformInput());
  EXPECT_EQ(SK_ColorGREEN, mixer.GetInputColor(kColorTest0));
}

// Tests that if GetInputColor() needs to reference the output of a previous
// mixer, it will reference the result color (i.e. after applying any recipe)
// rather than referencing that mixer's input color.
TEST(ColorMixerTest, GetInputColorRespectsRecipePreviousMixer) {
  ColorMixer mixer0;
  mixer0.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  mixer0[kColorTest0] = GetColorWithMaxContrast(FromTransformInput());
  ColorMixer mixer1(&mixer0);
  mixer1.AddSet({kColorSetTest1, {{kColorTest1, SK_ColorRED}}});
  EXPECT_EQ(color_utils::GetColorWithMaxContrast(SK_ColorGREEN),
            mixer1.GetInputColor(kColorTest0));
}

// Tests that GetOriginalColorFromSet() returns a placeholder color when there
// are no ColorSets.
TEST(ColorMixerTest, GetOriginalColorFromSetNoSets) {
  EXPECT_EQ(gfx::kPlaceholderColor,
            ColorMixer().GetOriginalColorFromSet(kColorTest0, kColorSetTest0));
}

// Tests that GetOriginalColorFromSet() will find the color in a single input
// set and return a placeholder color for a color not in that set.
TEST(ColorMixerTest, GetOriginalColorFromSetOneSet) {
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  EXPECT_EQ(SK_ColorGREEN,
            mixer.GetOriginalColorFromSet(kColorTest0, kColorSetTest0));
  EXPECT_EQ(gfx::kPlaceholderColor,
            mixer.GetOriginalColorFromSet(kColorTest1, kColorSetTest0));
  EXPECT_EQ(gfx::kPlaceholderColor,
            mixer.GetOriginalColorFromSet(kColorTest0, kColorSetTest1));
}

// Tests that GetOriginalColorFromSet() will find the colors in multiple
// non-overlapping input sets.
TEST(ColorMixerTest, GetOriginalColorFromSetTwoSetsNonOverlapping) {
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  mixer.AddSet({kColorSetTest1, {{kColorTest1, SK_ColorRED}}});
  EXPECT_EQ(SK_ColorGREEN,
            mixer.GetOriginalColorFromSet(kColorTest0, kColorSetTest0));
  EXPECT_EQ(gfx::kPlaceholderColor,
            mixer.GetOriginalColorFromSet(kColorTest1, kColorSetTest0));
  EXPECT_EQ(gfx::kPlaceholderColor,
            mixer.GetOriginalColorFromSet(kColorTest0, kColorSetTest1));
  EXPECT_EQ(SK_ColorRED,
            mixer.GetOriginalColorFromSet(kColorTest1, kColorSetTest1));
}

// Tests that when two input sets specify the same ID, GetOriginalColorFromSet()
// can retrieve either color, depending on the requested set.
TEST(ColorMixerTest, GetOriginalColorFromSetTwoSetsOverlapping) {
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  mixer.AddSet({kColorSetTest1, {{kColorTest0, SK_ColorRED}}});
  EXPECT_EQ(SK_ColorGREEN,
            mixer.GetOriginalColorFromSet(kColorTest0, kColorSetTest0));
  EXPECT_EQ(SK_ColorRED,
            mixer.GetOriginalColorFromSet(kColorTest0, kColorSetTest1));
}

// Tests that when a mixer does not have a requested color for
// GetOriginalColorFromSet(), it forwards the request to the previous mixer.
TEST(ColorMixerTest, GetOriginalColorFromSetPreviousMixer) {
  ColorMixer mixer0;
  mixer0.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  ColorMixer mixer1(&mixer0);
  mixer1.AddSet({kColorSetTest1, {{kColorTest1, SK_ColorRED}}});
  EXPECT_EQ(SK_ColorGREEN,
            mixer1.GetOriginalColorFromSet(kColorTest0, kColorSetTest0));
}

// Tests that GetOriginalColorFromSet() on a given mixer ignores any recipe that
// mixer has for that color.
TEST(ColorMixerTest, GetOriginalColorFromSetIgnoresRecipe) {
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  mixer[kColorTest0] = GetColorWithMaxContrast(FromTransformInput());
  EXPECT_EQ(SK_ColorGREEN,
            mixer.GetOriginalColorFromSet(kColorTest0, kColorSetTest0));
}

// Tests that if GetOriginalColorFromSet() needs to reference the input of a
// previous mixer, it will reference the input color (i.e. without applying any
// recipe) rather than referencing that mixer's result color.
TEST(ColorMixerTest, GetOriginalColorFromSetIgnoresRecipePreviousMixer) {
  ColorMixer mixer0;
  mixer0.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  mixer0[kColorTest0] = GetColorWithMaxContrast(FromTransformInput());
  ColorMixer mixer1(&mixer0);
  mixer1.AddSet({kColorSetTest1, {{kColorTest1, SK_ColorRED}}});
  EXPECT_EQ(SK_ColorGREEN,
            mixer1.GetOriginalColorFromSet(kColorTest0, kColorSetTest0));
}

// Tests that GetResultColor() returns a placeholder color when there are no
// ColorSets or recipes.
TEST(ColorMixerTest, GetResultColorNoInput) {
  EXPECT_EQ(gfx::kPlaceholderColor, ColorMixer().GetResultColor(kColorTest0));
}

// Tests that GetResultColor() returns its input color when there are no
// recipes.
TEST(ColorMixerTest, GetResultColorNoRecipe) {
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  EXPECT_EQ(SK_ColorGREEN, mixer.GetResultColor(kColorTest0));
}

// Tests that GetResultColor() does not require an input set to provide an
// initial value for its requested color.
TEST(ColorMixerTest, GetResultColorNoSet) {
  ColorMixer mixer;
  mixer[kColorTest0] = ColorTransform(SK_ColorGREEN);
  mixer[kColorTest1] = GetColorWithMaxContrast(FromTransformInput());
  EXPECT_EQ(SK_ColorGREEN, mixer.GetResultColor(kColorTest0));
  EXPECT_NE(gfx::kPlaceholderColor, mixer.GetResultColor(kColorTest1));
}

// Tests that having an initial value for a requested color will not affect the
// output of GetResultColor() if its recipe overrides it.
TEST(ColorMixerTest, GetResultColorIgnoresSet) {
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0,
                {{kColorTest0, SK_ColorWHITE}, {kColorTest1, SK_ColorBLACK}}});
  mixer[kColorTest0] = ColorTransform(SK_ColorGREEN);
  mixer[kColorTest1] = ColorTransform(SK_ColorGREEN);
  EXPECT_EQ(SK_ColorGREEN, mixer.GetResultColor(kColorTest0));
  EXPECT_EQ(SK_ColorGREEN, mixer.GetResultColor(kColorTest1));
}

// Tests that GetResultColor() can reference other result colors, each of which
// will have its recipe applied.
TEST(ColorMixerTest, GetResultColorChained) {
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest1, SK_ColorWHITE}}});
  mixer[kColorTest0] = ColorTransform(gfx::kGoogleBlue050);
  mixer[kColorTest1] = BlendTowardMaxContrast(
      GetColorWithMaxContrast(FromTransformInput()), 0x29);
  mixer[kColorTest2] =
      BlendForMinContrast(gfx::kGoogleBlue500, kColorTest1, kColorTest0);
  EXPECT_EQ(SkColorSetRGB(0x89, 0xB3, 0xF8), mixer.GetResultColor(kColorTest2));
}

}  // namespace
}  // namespace ui
