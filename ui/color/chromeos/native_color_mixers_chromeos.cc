// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#include "third_party/material_color_utilities/src/cpp/palettes/tones.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace ui {

// Defines mappings for colors used in Ash and Lacros. Colors that
// are only used in Ash are defined in //ash/style.
void AddNativeCoreColorMixer(ColorProvider* provider,
                             const ColorProviderKey& key) {
  ColorMixer& mixer = provider->AddMixer();
  mixer[kColorAshSystemUIMenuBackground] = {kColorMenuBackground};
  mixer[kColorAshSystemUIMenuIcon] = {kColorMenuIcon};
  mixer[kColorAshSystemUIMenuItemBackgroundSelected] = {
      kColorMenuItemBackgroundSelected};
  mixer[kColorAshSystemUIMenuSeparator] = {kColorMenuSeparator};
  mixer[kColorMultitaskMenuNudgePulse] = {kColorEndpointForeground};

  bool dark_mode = key.color_mode == ColorProviderKey::ColorMode::kDark;

  // Add color initializations for highlight border.
  {
    // TODO(b/291622042): Delete when Jelly is fully launched.
    const ui::ColorTransform light_border = {SkColorSetA(SK_ColorBLACK, 0x0F)};
    const auto default_background_color =
        ui::GetEndpointColorWithMinContrast({ui::kColorPrimaryBackground});
    const auto background_color =
        key.user_color.has_value() ? ui::ColorTransform(key.user_color.value())
                                   : default_background_color;
    mixer[kColorHighlightBorderBorder1] =
        dark_mode ? SetAlpha(background_color, SK_AlphaOPAQUE * 0.8f)
                  : light_border;
    mixer[kColorHighlightBorderBorder2] =
        dark_mode ? SetAlpha(background_color, SK_AlphaOPAQUE * 0.6f)
                  : light_border;
    mixer[kColorHighlightBorderBorder3] = light_border;

    mixer[kColorHighlightBorderHighlight1] = {
        SkColorSetA(SK_ColorWHITE, dark_mode ? 0x14 : 0x4C)};
    mixer[kColorHighlightBorderHighlight2] = {
        SkColorSetA(SK_ColorWHITE, dark_mode ? 0x0F : 0x33)};
    mixer[kColorHighlightBorderHighlight3] = {kColorHighlightBorderHighlight1};
  }

  // This matches cros.sys.system-highlight
  mixer[kColorCrosSystemHighlight] =
      ui::SetAlpha(ui::kColorRefNeutral100, dark_mode ? 0x0F : 0x28);
  // This matches cros.sys.system-highlight-border
  mixer[kColorCrosSystemHighlightBorder] =
      ui::SetAlpha(ui::kColorRefNeutral0, 0x14);
  // This matches cros.sys.system-highlight-border1
  mixer[kColorCrosSystemHighlightBorder1] =
      ui::SetAlpha(ui::kColorRefNeutral0, dark_mode ? 0x14 : 0x0F);

  // Seed color for cros.ref.green @ hue angle 217 i.e. Google Blue.
  material_color_utilities::TonalPalette green_palette(
      SkColorSetRGB(0x4F, 0xA8, 0x34));
  mixer[kColorCrosSysPositive] = {dark_mode ? green_palette.get(80)
                                            : green_palette.get(50)};

  // cros.ref.sparkle-complement @ 217.
  material_color_utilities::TonalPalette complement(
      SkColorSetRGB(0x40, 0x67, 0x43));
  mixer[kColorCrosSysComplementVariant] = {dark_mode ? complement.get(30)
                                                     : complement.get(95)};
}

}  // namespace ui
