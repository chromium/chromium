// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_test_ids.h"
#include "ui/gfx/color_palette.h"

namespace ui {
namespace {

// Tests that when there are no mixers, GetColor() returns a placeholder value.
TEST(ColorProviderTest, GetColorNoMixers) {
  EXPECT_EQ(gfx::kPlaceholderColor, ColorProvider().GetColor(kColorTest0));
}

// Tests that when there is a single mixer, GetColor() makes use of it where
// possible.
TEST(ColorProviderTest, SingleMixer) {
  ColorProvider provider;
  provider.AddMixer().AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  EXPECT_EQ(SK_ColorGREEN, provider.GetColor(kColorTest0));
  EXPECT_EQ(gfx::kPlaceholderColor, provider.GetColor(kColorTest1));
}

// Tests that when there are multiple non-overlapping mixers, GetColor() makes
// use of both.
TEST(ColorProviderTest, NonOverlappingMixers) {
  ColorProvider provider;
  provider.AddMixer().AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  provider.AddMixer().AddSet({kColorSetTest1, {{kColorTest1, SK_ColorRED}}});
  EXPECT_EQ(SK_ColorGREEN, provider.GetColor(kColorTest0));
  EXPECT_EQ(SK_ColorRED, provider.GetColor(kColorTest1));
}

// Tests that when mixers supply overlapping color specifications, the last one
// added takes priority.
TEST(ColorProviderTest, OverlappingMixers) {
  ColorProvider provider;
  provider.AddMixer().AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  provider.AddMixer().AddSet({kColorSetTest0, {{kColorTest0, SK_ColorRED}}});
  EXPECT_EQ(SK_ColorRED, provider.GetColor(kColorTest0));
}

// Tests that repeated calls for the same color do not produce incorrect values.
// This attempts to verify that nothing is badly wrong with color caching.
TEST(ColorProviderTest, Caching) {
  ColorProvider provider;
  provider.AddMixer().AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  EXPECT_EQ(SK_ColorGREEN, provider.GetColor(kColorTest0));
  EXPECT_EQ(SK_ColorGREEN, provider.GetColor(kColorTest0));
  provider.AddMixer().AddSet({kColorSetTest0, {{kColorTest0, SK_ColorRED}}});
  EXPECT_EQ(SK_ColorRED, provider.GetColor(kColorTest0));
}

}  // namespace
}  // namespace ui
