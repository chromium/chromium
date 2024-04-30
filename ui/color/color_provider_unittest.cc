// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_test_ids.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace ui {
namespace {

// Tests that when there are no mixers, GetColor() returns a placeholder value.
TEST(ColorProviderTest, GetColorNoMixers) {
  ColorProvider provider;
  EXPECT_EQ(gfx::kPlaceholderColor, provider.GetColor(kColorTest0));
}

// Tests that when there is a single mixer, GetColor() makes use of it where
// possible.
TEST(ColorProviderTest, SingleMixer) {
  ColorProvider provider;
  provider.AddMixer()[kColorTest0] = {SK_ColorGREEN};
  EXPECT_EQ(SK_ColorGREEN, provider.GetColor(kColorTest0));
  EXPECT_EQ(gfx::kPlaceholderColor, provider.GetColor(kColorTest1));
}

// Tests that when there are multiple non-overlapping mixers, GetColor() makes
// use of both.
TEST(ColorProviderTest, NonOverlappingMixers) {
  ColorProvider provider;
  provider.AddMixer()[kColorTest0] = {SK_ColorGREEN};
  provider.AddMixer()[kColorTest1] = {SK_ColorRED};
  EXPECT_EQ(SK_ColorGREEN, provider.GetColor(kColorTest0));
  EXPECT_EQ(SK_ColorRED, provider.GetColor(kColorTest1));
}

// Tests that when mixers supply overlapping color specifications, the last one
// added takes priority.
TEST(ColorProviderTest, OverlappingMixers) {
  ColorProvider provider;
  provider.AddMixer()[kColorTest0] = {SK_ColorGREEN};
  provider.AddMixer()[kColorTest0] = {SK_ColorRED};
  EXPECT_EQ(SK_ColorRED, provider.GetColor(kColorTest0));
}

// Tests that with both a standard and a "postprocessing" mixer, GetColor()
// takes both into account.
TEST(ColorProviderTest, WithProcessing) {
  ColorProvider provider;
  provider.AddMixer()[kColorTest0] = {SK_ColorBLACK};
  provider.AddPostprocessingMixer()[kColorTest0] =
      GetColorWithMaxContrast(FromTransformInput());
  EXPECT_EQ(SK_ColorWHITE, provider.GetColor(kColorTest0));
}

// A "postprocessing" mixer can be added before regular mixers. The result
// should be equivalent.
TEST(ColorProviderTest, WithProcessingAddedBeforeRegular) {
  ColorProvider provider;
  provider.AddPostprocessingMixer()[kColorTest0] =
      GetColorWithMaxContrast(FromTransformInput());
  provider.AddMixer()[kColorTest0] = {SK_ColorBLACK};
  EXPECT_EQ(SK_ColorWHITE, provider.GetColor(kColorTest0));
}

// Tests that if a color is redefined by a later mixer, an earlier mixer will
// "see" the result.
TEST(ColorProviderTest, Redefinition) {
  ColorProvider provider;
  ColorMixer& mixer0 = provider.AddMixer();
  mixer0[kColorTest0] = {SK_ColorBLACK};
  mixer0[kColorTest1] = AlphaBlend(SK_ColorRED, kColorTest0, 0x01);
  provider.AddMixer()[kColorTest0] = {SK_ColorWHITE};
  EXPECT_EQ(SK_ColorWHITE, provider.GetColor(kColorTest0));
  EXPECT_FALSE(color_utils::IsDark(provider.GetColor(kColorTest1)));
}

// Tests that "postprocessing" mixers are skipped for the purposes of color
// lookup during intermediate stages.
TEST(ColorProviderTest, RedefinitionWithProcessing) {
  ColorProvider provider;
  ColorMixer& mixer0 = provider.AddMixer();
  mixer0[kColorTest0] = {SK_ColorBLACK};
  mixer0[kColorTest1] = AlphaBlend(SK_ColorRED, kColorTest0, 0x01);
  provider.AddMixer()[kColorTest0] = {SK_ColorWHITE};
  provider.AddPostprocessingMixer()[kColorTest0] =
      GetColorWithMaxContrast(FromTransformInput());
  EXPECT_NE(SK_ColorWHITE, provider.GetColor(kColorTest0));
  EXPECT_FALSE(color_utils::IsDark(provider.GetColor(kColorTest1)));
}

TEST(ColorProviderTest, SetColorForTesting) {
  ColorProvider provider;
  provider.SetColorForTesting(kColorTest0, SK_ColorGREEN);
  EXPECT_EQ(SK_ColorGREEN, provider.GetColor(kColorTest0));
  EXPECT_EQ(gfx::kPlaceholderColor, provider.GetColor(kColorTest1));
  provider.SetColorForTesting(kColorTest1, SK_ColorBLUE);
  EXPECT_EQ(SK_ColorBLUE, provider.GetColor(kColorTest1));
}

}  // namespace
}  // namespace ui
