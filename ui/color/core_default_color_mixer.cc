// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/core_default_color_mixer.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_set.h"
#include "ui/gfx/color_palette.h"

namespace ui {

namespace {

// TODO(pkasting): Construct colors from contrast ratios
// TODO(pkasting): Construct palette from accent, key, tint/bg, shade/fg colors

ColorMixer& AddMixerForDarkMode(ColorProvider* provider, bool high_contrast) {
  ColorMixer& mixer = provider->AddMixer();
  mixer.AddSet({kColorSetCoreDefaults,
                {
                    {kColorAccent, gfx::kGoogleBlue300},
                    {kColorAlertHighSeverity, gfx::kGoogleRed300},
                    {kColorAlertLowSeverity, gfx::kGoogleGreen300},
                    {kColorAlertMediumSeverity, gfx::kGoogleYellow300},
                    {kColorMidground, gfx::kGoogleGrey800},
                    {kColorPrimaryBackground, SkColorSetRGB(0x29, 0x2A, 0x2D)},
                    {kColorPrimaryForeground, gfx::kGoogleGrey200},
                    {kColorSecondaryForeground, gfx::kGoogleGrey500},
                }});
  return mixer;
}

ColorMixer& AddMixerForLightMode(ColorProvider* provider, bool high_contrast) {
  ColorMixer& mixer = provider->AddMixer();
  mixer.AddSet({kColorSetCoreDefaults,
                {
                    {kColorAccent, gfx::kGoogleBlue600},
                    {kColorAlertHighSeverity, gfx::kGoogleRed600},
                    {kColorAlertLowSeverity, gfx::kGoogleGreen700},
                    {kColorAlertMediumSeverity, gfx::kGoogleYellow700},
                    {kColorMidground, gfx::kGoogleGrey300},
                    {kColorPrimaryBackground, SK_ColorWHITE},
                    {kColorPrimaryForeground, gfx::kGoogleGrey900},
                    {kColorSecondaryForeground, gfx::kGoogleGrey700},
                }});
  return mixer;
}

ColorTransform GoogleColorWithContrastRatio(ColorTransform foreground_transform,
                                            ColorTransform background_transform,
                                            float contrast_ratio) {
  const auto generator = [](ColorTransform foreground_transform,
                            ColorTransform background_transform,
                            float contrast_ratio, SkColor input_color,
                            const ColorMixer& mixer) {
    const SkColor foreground_color =
        foreground_transform.Run(input_color, mixer);
    const SkColor background_color =
        background_transform.Run(input_color, mixer);
    contrast_ratio *=
        color_utils::GetContrastRatio(foreground_color, background_color);
    const SkColor result_color = color_utils::PickGoogleColor(
        foreground_color, background_color, contrast_ratio);
    DVLOG(2) << "ColorTransform GoogleColorWithContrastRatio:"
             << " FG Transform Color: " << SkColorName(foreground_color)
             << " BG Transform Color: " << SkColorName(background_color)
             << " Contrast Ratio: " << base::NumberToString(contrast_ratio)
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(foreground_transform),
                             std::move(background_transform), contrast_ratio);
}

}  // namespace

void AddCoreDefaultColorMixer(ColorProvider* provider,
                              bool dark_window,
                              bool high_contrast) {
  DVLOG(2) << "Adding CoreDefaultColorMixer to ColorProvider for "
           << (dark_window ? "Dark" : "Light")
           << (high_contrast ? " High Contrast" : "") << " window.";
  ColorMixer& mixer = dark_window
                          ? AddMixerForDarkMode(provider, high_contrast)
                          : AddMixerForLightMode(provider, high_contrast);
  mixer[kColorDisabledForeground] = BlendForMinContrast(
      gfx::kGoogleGrey600, kColorPrimaryBackground, kColorPrimaryForeground);
  mixer[kColorEndpointBackground] =
      GetColorWithMaxContrast(kColorEndpointForeground);
  mixer[kColorEndpointForeground] =
      GetColorWithMaxContrast(kColorPrimaryBackground);
  // This produces light and dark item highlight colors of blue 500 and 400,
  // respectively.
  mixer[kColorItemHighlight] = GoogleColorWithContrastRatio(
      kColorAccent, kColorPrimaryBackground, 0.75f);
  mixer[kColorItemSelectionBackground] =
      AlphaBlend(kColorAccent, kColorPrimaryBackground, 0x3C);
  mixer[kColorMenuSelectionBackground] =
      AlphaBlend(kColorEndpointForeground, kColorPrimaryBackground,
                 gfx::kGoogleGreyAlpha200);
  mixer[kColorSubtleAccent] = AlphaBlend(kColorAccent, kColorPrimaryBackground,
                                         gfx::kGoogleGreyAlpha400);
  mixer[kColorSubtleEmphasisBackground] =
      BlendTowardMaxContrast(kColorPrimaryBackground, gfx::kGoogleGreyAlpha100);
  mixer[kColorTextSelectionBackground] = AlphaBlend(
      kColorAccent, kColorPrimaryBackground, gfx::kGoogleGreyAlpha500);
  mixer[kColorTextSelectionForeground] =
      GetColorWithMaxContrast(kColorTextSelectionBackground);
}

}  // namespace ui
