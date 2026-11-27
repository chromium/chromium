// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/dynamic_color/palette_factory.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/dynamic_color/palette.h"

namespace ui {
namespace {

TEST(PaletteFactoryTest, BlackSeedStepsAboveBlackToAvoidPinkFallback) {
  std::unique_ptr<Palette> black_palette = GeneratePalette(
      SK_ColorBLACK, ColorProviderKey::SchemeVariant::kTonalSpot);
  std::unique_ptr<Palette> near_black_palette =
      GeneratePalette(SkColorSetRGB(0x01, 0x01, 0x01),
                      ColorProviderKey::SchemeVariant::kTonalSpot);
  EXPECT_EQ(black_palette->primary().get(40),
            near_black_palette->primary().get(40));
  EXPECT_EQ(black_palette->primary().get(40), SkColorSetRGB(0x00, 0x68, 0x74));
}

}  // namespace
}  // namespace ui
