// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/dynamic_color/palette_factory.h"

#include <array>
#include <optional>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"

namespace ui {
namespace {

TEST(PaletteFactoryTest, GenerateStandardShadesForHueEqualsNullopt) {
  // For hue = std::nullopt, we expect shades of grey
  constexpr std::array<SkColor, kGeneratedShadesCount> kExpectedShades = {
      gfx::kGoogleGrey050,
      gfx::kGoogleGrey100,
      gfx::kGoogleGrey200,
      gfx::kGoogleGrey300,
      gfx::kGoogleGrey400,
      gfx::kGoogleGrey500,
      gfx::kGoogleGrey600,
      gfx::kGoogleGrey700,
      gfx::kGoogleGrey800,
      gfx::kGoogleGrey900,
      SkColorSetRGB(0x4C, 0x4D, 0x4E)  // Shade 1000
  };

  std::array<SkColor, kGeneratedShadesCount> actualShades{};

  // Populate |actualShades| array for hue = std::nullopt
  GenerateStandardShadesFromHue(std::nullopt, actualShades);

  for (size_t i = 0; i < kGeneratedShadesCount; ++i) {
    EXPECT_EQ(kExpectedShades[i], actualShades[i])
        << "Mismatch at shade index " << i;
  }
}

TEST(PaletteFactoryTest, GenerateStandardShadesForHueNotEqualsNullopt) {
  // Expected standard shades for hue = 40
  constexpr std::array<SkColor, kGeneratedShadesCount> kExpectedShades = {
      SkColorSetRGB(0xFF, 0xED, 0xE7),  // Shade 50
      SkColorSetRGB(0xFF, 0xDC, 0xD0),  // Shade 100
      SkColorSetRGB(0xFF, 0xC1, 0xAA),  // Shade 200
      SkColorSetRGB(0xFF, 0xA6, 0x83),  // Shade 300
      SkColorSetRGB(0xFF, 0x87, 0x56),  // Shade 400
      SkColorSetRGB(0xFA, 0x6E, 0x2F),  // Shade 500
      SkColorSetRGB(0xE4, 0x5F, 0x20),  // Shade 600
      SkColorSetRGB(0xCB, 0x52, 0x19),  // Shade 700
      SkColorSetRGB(0xB3, 0x46, 0x11),  // Shade 800
      SkColorSetRGB(0x9F, 0x3D, 0x0E),  // Shade 900
      SkColorSetRGB(0x5A, 0x3D, 0x32),  // Shade 1000
  };

  std::array<SkColor, kGeneratedShadesCount> actualShades{};

  // Populate |actualShades| array for hue = 40
  GenerateStandardShadesFromHue(40, actualShades);

  for (size_t i = 0; i < kGeneratedShadesCount; ++i) {
    EXPECT_EQ(kExpectedShades[i], actualShades[i])
        << "Mismatch at shade index " << i;
  }
}

}  // namespace
}  // namespace ui
