// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_recipe.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_set.h"
#include "ui/color/color_test_ids.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"

namespace ui {
namespace {

// Tests that a recipe with no transforms passes through its input color
// unchanged.
TEST(ColorRecipeTest, EmptyRecipeIsPassthrough) {
  const ColorRecipe recipe;
  const auto verify_passthrough = [&](SkColor input) {
    EXPECT_EQ(input, recipe.GenerateResult(input, ColorMixer()));
  };
  verify_passthrough(SK_ColorBLACK);
  verify_passthrough(SK_ColorWHITE);
  verify_passthrough(SK_ColorRED);
}

// Tests that a transform in a recipe has an effect.
TEST(ColorRecipeTest, OneTransform) {
  constexpr SkColor kOutput = SK_ColorGREEN;
  ColorRecipe recipe = {kOutput};
  const auto verify_transform = [&](SkColor input) {
    EXPECT_EQ(kOutput, recipe.GenerateResult(input, ColorMixer()));
  };
  verify_transform(SK_ColorBLACK);
  verify_transform(SK_ColorWHITE);
  verify_transform(SK_ColorRED);
}

// Tests that in a recipe with multiple transforms, each is applied.
TEST(ColorRecipeTest, ChainedTransforms) {
  ColorRecipe recipe = DeriveDefaultIconColor(FromTransformInput()) +
                       BlendForMinContrast(FromTransformInput(), kColorTest0);
  constexpr SkColor kBackground = SK_ColorWHITE;
  ColorMixer mixer;
  mixer.AddSet({kColorSetTest0, {{kColorTest0, kBackground}}});
  const auto verify_chain = [&](SkColor input) {
    const SkColor color = recipe.GenerateResult(input, mixer);
    // The DeriveDefaultIconColor transform should change the output color even
    // when the BlendForMinContrast transform takes no action.
    EXPECT_NE(input, color);
    // The BlendForMinContrast transform should always be able to guarantee
    // readable contrast against white.
    EXPECT_GE(color_utils::GetContrastRatio(color, kBackground),
              color_utils::kMinimumReadableContrastRatio);
  };
  verify_chain(SK_ColorBLACK);
  verify_chain(SK_ColorWHITE);
  verify_chain(SK_ColorRED);
}

}  // namespace
}  // namespace ui
