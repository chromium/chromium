// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/ref_color_mixer.h"

#include <memory>

#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/dynamic_color/palette.h"
#include "ui/color/dynamic_color/palette_factory.h"
#include "ui/gfx/color_palette.h"

namespace ui {

// The baseline palette for ref colors. This should be used in the absence of a
// defined user_color.
void AddBaselinePalette(ColorProvider* provider) {
  ColorMixer& mixer = provider->AddMixer();
  mixer[kColorRefPrimary0] = {SkColorSetRGB(0x00, 0x00, 0x00)};
  mixer[kColorRefPrimary10] = {SkColorSetRGB(0x04, 0x1E, 0x49)};
  mixer[kColorRefPrimary20] = {SkColorSetRGB(0x06, 0x2E, 0x6F)};
  mixer[kColorRefPrimary25] = {SkColorSetRGB(0x07, 0x38, 0x88)};
  mixer[kColorRefPrimary30] = {SkColorSetRGB(0x08, 0x42, 0xA0)};
  mixer[kColorRefPrimary40] = {SkColorSetRGB(0x0B, 0x57, 0xD0)};
  mixer[kColorRefPrimary50] = {SkColorSetRGB(0x1B, 0x6E, 0xF3)};
  mixer[kColorRefPrimary60] = {SkColorSetRGB(0x4C, 0x8D, 0xF6)};
  mixer[kColorRefPrimary70] = {SkColorSetRGB(0x7C, 0xAC, 0xF8)};
  mixer[kColorRefPrimary80] = {SkColorSetRGB(0xA8, 0xC7, 0xFA)};
  mixer[kColorRefPrimary90] = {SkColorSetRGB(0xD3, 0xE3, 0xFD)};
  mixer[kColorRefPrimary95] = {SkColorSetRGB(0xEC, 0xF3, 0xFE)};
  mixer[kColorRefPrimary99] = {SkColorSetRGB(0xFA, 0xFB, 0xFF)};
  mixer[kColorRefPrimary100] = {SkColorSetRGB(0xFF, 0xFF, 0xFF)};

  mixer[kColorRefSecondary0] = {SkColorSetRGB(0x00, 0x00, 0x00)};
  mixer[kColorRefSecondary10] = {SkColorSetRGB(0x00, 0x1D, 0x35)};
  mixer[kColorRefSecondary12] = {SkColorSetRGB(0x00, 0x22, 0x38)};
  mixer[kColorRefSecondary15] = {SkColorSetRGB(0x00, 0x28, 0x45)};
  mixer[kColorRefSecondary20] = {SkColorSetRGB(0x00, 0x33, 0x55)};
  mixer[kColorRefSecondary25] = {SkColorSetRGB(0x00, 0x3f, 0x66)};
  mixer[kColorRefSecondary30] = {SkColorSetRGB(0x00, 0x4A, 0x77)};
  mixer[kColorRefSecondary35] = {SkColorSetRGB(0x00, 0x57, 0x89)};
  mixer[kColorRefSecondary40] = {SkColorSetRGB(0x00, 0x63, 0x9B)};
  mixer[kColorRefSecondary50] = {SkColorSetRGB(0x04, 0x7D, 0xB7)};
  mixer[kColorRefSecondary60] = {SkColorSetRGB(0x39, 0x98, 0xD3)};
  mixer[kColorRefSecondary70] = {SkColorSetRGB(0x5A, 0xB3, 0xF0)};
  mixer[kColorRefSecondary80] = {SkColorSetRGB(0x7F, 0xCF, 0xFF)};
  mixer[kColorRefSecondary90] = {SkColorSetRGB(0xC2, 0xE7, 0xFF)};
  mixer[kColorRefSecondary95] = {SkColorSetRGB(0xDF, 0xF3, 0xFF)};
  mixer[kColorRefSecondary99] = {SkColorSetRGB(0xF7, 0xFC, 0xFF)};
  mixer[kColorRefSecondary100] = {SkColorSetRGB(0xFF, 0xFF, 0xFF)};

  mixer[kColorRefTertiary0] = {SkColorSetRGB(0x00, 0x00, 0x00)};
  mixer[kColorRefTertiary10] = {SkColorSetRGB(0x07, 0x27, 0x11)};
  mixer[kColorRefTertiary20] = {SkColorSetRGB(0x0A, 0x38, 0x18)};
  mixer[kColorRefTertiary30] = {SkColorSetRGB(0x0F, 0x52, 0x23)};
  mixer[kColorRefTertiary40] = {SkColorSetRGB(0x14, 0x6C, 0x2E)};
  mixer[kColorRefTertiary50] = {SkColorSetRGB(0x19, 0x86, 0x39)};
  mixer[kColorRefTertiary60] = {SkColorSetRGB(0x1E, 0xA4, 0x46)};
  mixer[kColorRefTertiary70] = {SkColorSetRGB(0x37, 0xBE, 0x5F)};
  mixer[kColorRefTertiary80] = {SkColorSetRGB(0x6D, 0xD5, 0x8C)};
  mixer[kColorRefTertiary90] = {SkColorSetRGB(0xC4, 0xEE, 0xD0)};
  mixer[kColorRefTertiary95] = {SkColorSetRGB(0xE7, 0xF8, 0xED)};
  mixer[kColorRefTertiary99] = {SkColorSetRGB(0xF2, 0xFF, 0xEE)};
  mixer[kColorRefTertiary100] = {SkColorSetRGB(0xFF, 0xFF, 0xFF)};

  mixer[kColorRefError0] = {SkColorSetRGB(0x00, 0x00, 0x00)};
  mixer[kColorRefError10] = {SkColorSetRGB(0x41, 0x0E, 0x0B)};
  mixer[kColorRefError20] = {SkColorSetRGB(0x60, 0x14, 0x10)};
  mixer[kColorRefError30] = {SkColorSetRGB(0x8C, 0x1D, 0x18)};
  mixer[kColorRefError40] = {SkColorSetRGB(0xB3, 0x26, 0x1E)};
  mixer[kColorRefError50] = {SkColorSetRGB(0xDC, 0x36, 0x2E)};
  mixer[kColorRefError60] = {SkColorSetRGB(0xE4, 0x69, 0x62)};
  mixer[kColorRefError70] = {SkColorSetRGB(0xEC, 0x92, 0x8E)};
  mixer[kColorRefError80] = {SkColorSetRGB(0xF2, 0xB8, 0xB5)};
  mixer[kColorRefError90] = {SkColorSetRGB(0xF9, 0xDE, 0xDC)};
  mixer[kColorRefError95] = {SkColorSetRGB(0xFC, 0xEE, 0xEE)};
  mixer[kColorRefError99] = {SkColorSetRGB(0xFF, 0xFB, 0xF9)};
  mixer[kColorRefError100] = {SkColorSetRGB(0xFF, 0xFF, 0xFF)};

  mixer[kColorRefNeutral0] = {SkColorSetRGB(0x00, 0x00, 0x00)};
  mixer[kColorRefNeutral4] = {SkColorSetRGB(0x0E, 0x0E, 0x0F)};
  mixer[kColorRefNeutral6] = {SkColorSetRGB(0x13, 0x13, 0x14)};
  mixer[kColorRefNeutral8] = {SkColorSetRGB(0x16, 0x18, 0x18)};
  mixer[kColorRefNeutral10] = {SkColorSetRGB(0x1F, 0x1F, 0x1F)};
  mixer[kColorRefNeutral12] = {SkColorSetRGB(0x1F, 0x20, 0x20)};
  mixer[kColorRefNeutral15] = {SkColorSetRGB(0x28, 0x28, 0x28)};
  mixer[kColorRefNeutral17] = {SkColorSetRGB(0x2A, 0x2A, 0x2A)};
  mixer[kColorRefNeutral20] = {SkColorSetRGB(0x30, 0x30, 0x30)};
  mixer[kColorRefNeutral22] = {SkColorSetRGB(0x34, 0x35, 0x35)};
  mixer[kColorRefNeutral24] = {SkColorSetRGB(0x39, 0x39, 0x39)};
  mixer[kColorRefNeutral25] = {SkColorSetRGB(0x3c, 0x3c, 0x3c)};
  mixer[kColorRefNeutral30] = {SkColorSetRGB(0x47, 0x47, 0x47)};
  mixer[kColorRefNeutral40] = {SkColorSetRGB(0x5E, 0x5E, 0x5E)};
  mixer[kColorRefNeutral50] = {SkColorSetRGB(0x75, 0x75, 0x75)};
  mixer[kColorRefNeutral60] = {SkColorSetRGB(0x8F, 0x8F, 0x8F)};
  mixer[kColorRefNeutral70] = {SkColorSetRGB(0xAB, 0xAB, 0xAB)};
  mixer[kColorRefNeutral80] = {SkColorSetRGB(0xC7, 0xC7, 0xC7)};
  mixer[kColorRefNeutral87] = {SkColorSetRGB(0xDA, 0xDA, 0xDA)};
  mixer[kColorRefNeutral90] = {SkColorSetRGB(0xE3, 0xE3, 0xE3)};
  mixer[kColorRefNeutral92] = {SkColorSetRGB(0xE9, 0xE8, 0xE8)};
  mixer[kColorRefNeutral94] = {SkColorSetRGB(0xEF, 0xED, 0xED)};
  mixer[kColorRefNeutral95] = {SkColorSetRGB(0xF2, 0xF2, 0xF2)};
  mixer[kColorRefNeutral96] = {SkColorSetRGB(0xF4, 0xF3, 0xF2)};
  mixer[kColorRefNeutral98] = {SkColorSetRGB(0xFA, 0xF9, 0xF8)};
  mixer[kColorRefNeutral99] = {SkColorSetRGB(0xFD, 0xFC, 0xFB)};
  mixer[kColorRefNeutral100] = {SkColorSetRGB(0xFF, 0xFF, 0xFF)};

  mixer[kColorRefNeutralVariant0] = {SkColorSetRGB(0x00, 0x00, 0x00)};
  mixer[kColorRefNeutralVariant10] = {SkColorSetRGB(0x19, 0x1D, 0x1C)};
  mixer[kColorRefNeutralVariant15] = {SkColorSetRGB(0x23, 0x27, 0x26)};
  mixer[kColorRefNeutralVariant20] = {SkColorSetRGB(0x2D, 0x31, 0x2F)};
  mixer[kColorRefNeutralVariant30] = {SkColorSetRGB(0x44, 0x47, 0x46)};
  mixer[kColorRefNeutralVariant40] = {SkColorSetRGB(0x5C, 0x5F, 0x5E)};
  mixer[kColorRefNeutralVariant50] = {SkColorSetRGB(0x74, 0x77, 0x75)};
  mixer[kColorRefNeutralVariant60] = {SkColorSetRGB(0x8E, 0x91, 0x8F)};
  mixer[kColorRefNeutralVariant70] = {SkColorSetRGB(0xA9, 0xAC, 0xAA)};
  mixer[kColorRefNeutralVariant80] = {SkColorSetRGB(0xC4, 0xC7, 0xC5)};
  mixer[kColorRefNeutralVariant90] = {SkColorSetRGB(0xE1, 0xE3, 0xE1)};
  mixer[kColorRefNeutralVariant95] = {SkColorSetRGB(0xEF, 0xF2, 0xEF)};
  mixer[kColorRefNeutralVariant99] = {SkColorSetRGB(0xFA, 0xFD, 0xFB)};
  mixer[kColorRefNeutralVariant100] = {SkColorSetRGB(0xFF, 0xFF, 0xFF)};
}

// Adds the dynamic color palette tokens based on user_color. This is the base
// palette so it is independent of ColorMode.
void AddGeneratedPalette(ColorProvider* provider,
                         SkColor seed_color,
                         ColorProviderKey::SchemeVariant variant) {
  std::unique_ptr<Palette> palette = GeneratePalette(seed_color, variant);

  ColorMixer& mixer = provider->AddMixer();
  mixer[kColorRefPrimary0] = {palette->primary().get(0)};
  mixer[kColorRefPrimary10] = {palette->primary().get(10)};
  mixer[kColorRefPrimary20] = {palette->primary().get(20)};
  mixer[kColorRefPrimary25] = {palette->primary().get(25)};
  mixer[kColorRefPrimary30] = {palette->primary().get(30)};
  mixer[kColorRefPrimary40] = {palette->primary().get(40)};
  mixer[kColorRefPrimary50] = {palette->primary().get(50)};
  mixer[kColorRefPrimary60] = {palette->primary().get(60)};
  mixer[kColorRefPrimary70] = {palette->primary().get(70)};
  mixer[kColorRefPrimary80] = {palette->primary().get(80)};
  mixer[kColorRefPrimary90] = {palette->primary().get(90)};
  mixer[kColorRefPrimary95] = {palette->primary().get(95)};
  mixer[kColorRefPrimary99] = {palette->primary().get(99)};
  mixer[kColorRefPrimary100] = {palette->primary().get(100)};

  mixer[kColorRefSecondary0] = {palette->secondary().get(0)};
  mixer[kColorRefSecondary10] = {palette->secondary().get(10)};
  mixer[kColorRefSecondary12] = {palette->secondary().get(12)};
  mixer[kColorRefSecondary15] = {palette->secondary().get(15)};
  mixer[kColorRefSecondary20] = {palette->secondary().get(20)};
  mixer[kColorRefSecondary25] = {palette->secondary().get(25)};
  mixer[kColorRefSecondary30] = {palette->secondary().get(30)};
  mixer[kColorRefSecondary35] = {palette->secondary().get(35)};
  mixer[kColorRefSecondary40] = {palette->secondary().get(40)};
  mixer[kColorRefSecondary50] = {palette->secondary().get(50)};
  mixer[kColorRefSecondary60] = {palette->secondary().get(60)};
  mixer[kColorRefSecondary70] = {palette->secondary().get(70)};
  mixer[kColorRefSecondary80] = {palette->secondary().get(80)};
  mixer[kColorRefSecondary90] = {palette->secondary().get(90)};
  mixer[kColorRefSecondary95] = {palette->secondary().get(95)};
  mixer[kColorRefSecondary99] = {palette->secondary().get(99)};
  mixer[kColorRefSecondary100] = {palette->secondary().get(100)};

  mixer[kColorRefTertiary0] = {palette->tertiary().get(0)};
  mixer[kColorRefTertiary10] = {palette->tertiary().get(10)};
  mixer[kColorRefTertiary20] = {palette->tertiary().get(20)};
  mixer[kColorRefTertiary30] = {palette->tertiary().get(30)};
  mixer[kColorRefTertiary40] = {palette->tertiary().get(40)};
  mixer[kColorRefTertiary50] = {palette->tertiary().get(50)};
  mixer[kColorRefTertiary60] = {palette->tertiary().get(60)};
  mixer[kColorRefTertiary70] = {palette->tertiary().get(70)};
  mixer[kColorRefTertiary80] = {palette->tertiary().get(80)};
  mixer[kColorRefTertiary90] = {palette->tertiary().get(90)};
  mixer[kColorRefTertiary95] = {palette->tertiary().get(95)};
  mixer[kColorRefTertiary99] = {palette->tertiary().get(99)};
  mixer[kColorRefTertiary100] = {palette->tertiary().get(100)};

  mixer[kColorRefError0] = {palette->error().get(0)};
  mixer[kColorRefError10] = {palette->error().get(10)};
  mixer[kColorRefError20] = {palette->error().get(20)};
  mixer[kColorRefError30] = {palette->error().get(30)};
  mixer[kColorRefError40] = {palette->error().get(40)};
  mixer[kColorRefError50] = {palette->error().get(50)};
  mixer[kColorRefError60] = {palette->error().get(60)};
  mixer[kColorRefError70] = {palette->error().get(70)};
  mixer[kColorRefError80] = {palette->error().get(80)};
  mixer[kColorRefError90] = {palette->error().get(90)};
  mixer[kColorRefError95] = {palette->error().get(95)};
  mixer[kColorRefError99] = {palette->error().get(99)};
  mixer[kColorRefError100] = {palette->error().get(100)};

  mixer[kColorRefNeutral0] = {palette->neutral().get(0)};
  mixer[kColorRefNeutral4] = {palette->neutral().get(4)};
  mixer[kColorRefNeutral6] = {palette->neutral().get(6)};
  mixer[kColorRefNeutral8] = {palette->neutral().get(8)};
  mixer[kColorRefNeutral10] = {palette->neutral().get(10)};
  mixer[kColorRefNeutral12] = {palette->neutral().get(12)};
  mixer[kColorRefNeutral15] = {palette->neutral().get(15)};
  mixer[kColorRefNeutral17] = {palette->neutral().get(17)};
  mixer[kColorRefNeutral20] = {palette->neutral().get(20)};
  mixer[kColorRefNeutral22] = {palette->neutral().get(22)};
  mixer[kColorRefNeutral24] = {palette->neutral().get(24)};
  mixer[kColorRefNeutral25] = {palette->neutral().get(25)};
  mixer[kColorRefNeutral30] = {palette->neutral().get(30)};
  mixer[kColorRefNeutral40] = {palette->neutral().get(40)};
  mixer[kColorRefNeutral50] = {palette->neutral().get(50)};
  mixer[kColorRefNeutral60] = {palette->neutral().get(60)};
  mixer[kColorRefNeutral70] = {palette->neutral().get(70)};
  mixer[kColorRefNeutral80] = {palette->neutral().get(80)};
  mixer[kColorRefNeutral87] = {palette->neutral().get(87)};
  mixer[kColorRefNeutral90] = {palette->neutral().get(90)};
  mixer[kColorRefNeutral92] = {palette->neutral().get(92)};
  mixer[kColorRefNeutral94] = {palette->neutral().get(94)};
  mixer[kColorRefNeutral95] = {palette->neutral().get(95)};
  mixer[kColorRefNeutral96] = {palette->neutral().get(96)};
  mixer[kColorRefNeutral98] = {palette->neutral().get(98)};
  mixer[kColorRefNeutral99] = {palette->neutral().get(99)};
  mixer[kColorRefNeutral100] = {palette->neutral().get(100)};

  mixer[kColorRefNeutralVariant0] = {palette->neutral_variant().get(0)};
  mixer[kColorRefNeutralVariant10] = {palette->neutral_variant().get(10)};
  mixer[kColorRefNeutralVariant15] = {palette->neutral_variant().get(15)};
  mixer[kColorRefNeutralVariant20] = {palette->neutral_variant().get(20)};
  mixer[kColorRefNeutralVariant30] = {palette->neutral_variant().get(30)};
  mixer[kColorRefNeutralVariant40] = {palette->neutral_variant().get(40)};
  mixer[kColorRefNeutralVariant50] = {palette->neutral_variant().get(50)};
  mixer[kColorRefNeutralVariant60] = {palette->neutral_variant().get(60)};
  mixer[kColorRefNeutralVariant70] = {palette->neutral_variant().get(70)};
  mixer[kColorRefNeutralVariant80] = {palette->neutral_variant().get(80)};
  mixer[kColorRefNeutralVariant90] = {palette->neutral_variant().get(90)};
  mixer[kColorRefNeutralVariant95] = {palette->neutral_variant().get(95)};
  mixer[kColorRefNeutralVariant99] = {palette->neutral_variant().get(99)};
  mixer[kColorRefNeutralVariant100] = {palette->neutral_variant().get(100)};
}

void AddRefColorMixer(ColorProvider* provider, const ColorProviderKey& key) {
  // Typically user_color should always be set when the source has been set to
  // kAccent, however there may still be cases when this can occur (e.g. failing
  // to retrieve the system accent color on Windows).
  // TODO(tluk): Investigate guaranteeing the user_color is defined when kAccent
  // is set.
  if (!key.user_color.has_value() ||
      key.user_color_source == ColorProviderKey::UserColorSource::kBaseline ||
      key.user_color_source == ColorProviderKey::UserColorSource::kGrayscale) {
    AddBaselinePalette(provider);
  } else {
    // The default value for schemes is Tonal Spot.
    auto variant = key.scheme_variant.value_or(
        ColorProviderKey::SchemeVariant::kTonalSpot);

    // If the user color is set to black libmonet will default to a pink primary
    // color. This results in an unexpected user experience where all other
    // shades of gray result in the default blue primary color. To avoid this
    // edge case set the user_color one step above black to ensure the primary
    // blue is used, see crbug.com/1457314.
    SkColor user_color = key.user_color.value();
    if (user_color == SK_ColorBLACK) {
      user_color = SkColorSetRGB(0x01, 0x01, 0x01);
    }

    AddGeneratedPalette(provider, user_color, variant);
  }
}

}  // namespace ui
