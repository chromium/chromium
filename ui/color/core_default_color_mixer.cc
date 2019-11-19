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
           {kColorBodyForeground, gfx::kGoogleGrey500},
           {kColorLinkForeground, gfx::kGoogleBlue300},
           {kColorPrimaryBackground, SkColorSetRGB(0x29, 0x2A, 0x2D)},
           {kColorPrimaryForeground, gfx::kGoogleGrey200},
           {kColorSecondaryBackgroundSubtle, SkColorSetRGB(0x32, 0x36, 0x39)},
           {kColorSeparatorForeground, gfx::kGoogleGrey800},
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
                    {kColorBodyForeground, gfx::kGoogleGrey700},
                    {kColorLinkForeground, gfx::kGoogleBlue700},
                    {kColorPrimaryBackground, SK_ColorWHITE},
                    {kColorPrimaryForeground, gfx::kGoogleGrey900},
                    {kColorSecondaryBackgroundSubtle, gfx::kGoogleGrey050},
                    {kColorSeparatorForeground, gfx::kGoogleGrey300},
                    {kColorTextSelectionBackground, gfx::kGoogleBlue200},
                }});
  return mixer;
}

}  // namespace

void AddCoreDefaultColorMixers(ColorProvider* provider, bool dark_window) {
  ColorMixer& mixer = dark_window ? AddMixerForDarkMode(provider)
                                  : AddMixerForLightMode(provider);
  mixer[kColorSecondaryBackground] =
      BlendForMinContrastWithSelf(kColorPrimaryBackground, 1.67f);
  mixer[kColorSecondaryForeground] = BlendForMinContrast(
      gfx::kGoogleGrey600, kColorPrimaryBackground, kColorPrimaryForeground);
  // TODO(pkasting): High contrast?
}

}  // namespace ui
