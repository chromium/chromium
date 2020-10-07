// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_set.h"
#include "ui/gfx/color_palette.h"

namespace ui {

namespace {

// TODO(pkasting): Construct colors from contrast ratios
// TODO(pkasting): Construct palette from accent, key, tint/bg, shade/fg colors

ColorMixer& AddMixerForDarkMode(ColorProvider* provider) {
  ColorMixer& mixer = provider->AddMixer();
  mixer.AddSet(
      {kColorSetCoreDefaults,
       {
           {kColorAccent, gfx::kGoogleBlue300},
           {kColorAlertHighSeverity, gfx::kGoogleRed300},
           {kColorAlertLowSeverity, gfx::kGoogleGreen300},
           {kColorAlertMediumSeverity, gfx::kGoogleYellow300},
           {kColorMidground, gfx::kGoogleGrey800},
           {kColorPrimaryBackground, SkColorSetRGB(0x29, 0x2A, 0x2D)},
           {kColorPrimaryForeground, gfx::kGoogleGrey200},
           {kColorSecondaryForeground, gfx::kGoogleGrey500},
           {kColorSubtleEmphasisBackground, SkColorSetRGB(0x32, 0x36, 0x39)},
           {kColorTextSelectionBackground, gfx::kGoogleBlue800},
       }});
  return mixer;
}

ColorMixer& AddMixerForLightMode(ColorProvider* provider) {
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
                    {kColorSubtleEmphasisBackground, gfx::kGoogleGrey050},
                    {kColorTextSelectionBackground, gfx::kGoogleBlue200},
                }});
  return mixer;
}

}  // namespace

void AddCoreDefaultColorMixer(ColorProvider* provider, bool dark_window) {
  ColorMixer& mixer = dark_window ? AddMixerForDarkMode(provider)
                                  : AddMixerForLightMode(provider);
  mixer[kColorDisabledForeground] = BlendForMinContrast(
      gfx::kGoogleGrey600, kColorPrimaryBackground, kColorPrimaryForeground);
  mixer[kColorItemSelectionBackground] =
      BlendForMinContrastWithSelf(kColorPrimaryBackground, 1.67f);
  // TODO(pkasting): High contrast?
}

}  // namespace ui
