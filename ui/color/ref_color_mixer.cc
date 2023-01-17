// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/ref_color_mixer.h"

#include "base/logging.h"
#include "third_party/material_color_utilities/src/cpp/palettes/core.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/temp_palette.h"
#include "ui/gfx/color_palette.h"

namespace ui {

// Adds the dynamic color palette tokens based on user_color. This is the base
// palette so it is independent of ColorMode.
void AddRefColorMixer(ColorProvider* provider,
                      const ColorProviderManager::Key& key) {
  // TODO(skau): Before this launches, make sure this is always populated.
  SkColor seed_color = key.user_color.value_or(gfx::kGoogleBlue400);

  auto palette = material_color_utilities::CorePalette::Of(seed_color);

  ColorMixer& mixer = provider->AddMixer();
  mixer[kColorRefPrimary0] = {palette.primary().get(0)};
  mixer[kColorRefPrimary10] = {palette.primary().get(10)};
  mixer[kColorRefPrimary20] = {palette.primary().get(20)};
  mixer[kColorRefPrimary30] = {palette.primary().get(30)};
  mixer[kColorRefPrimary40] = {palette.primary().get(40)};
  mixer[kColorRefPrimary50] = {palette.primary().get(50)};
  mixer[kColorRefPrimary60] = {palette.primary().get(60)};
  mixer[kColorRefPrimary70] = {palette.primary().get(70)};
  mixer[kColorRefPrimary80] = {palette.primary().get(80)};
  mixer[kColorRefPrimary90] = {palette.primary().get(90)};
  mixer[kColorRefPrimary95] = {palette.primary().get(95)};
  mixer[kColorRefPrimary99] = {palette.primary().get(99)};
  mixer[kColorRefPrimary100] = {palette.primary().get(100)};

  mixer[kColorRefSecondary0] = {palette.secondary().get(0)};
  mixer[kColorRefSecondary10] = {palette.secondary().get(10)};
  mixer[kColorRefSecondary20] = {palette.secondary().get(20)};
  mixer[kColorRefSecondary30] = {palette.secondary().get(30)};
  mixer[kColorRefSecondary40] = {palette.secondary().get(40)};
  mixer[kColorRefSecondary50] = {palette.secondary().get(50)};
  mixer[kColorRefSecondary60] = {palette.secondary().get(60)};
  mixer[kColorRefSecondary70] = {palette.secondary().get(70)};
  mixer[kColorRefSecondary80] = {palette.secondary().get(80)};
  mixer[kColorRefSecondary90] = {palette.secondary().get(90)};
  mixer[kColorRefSecondary95] = {palette.secondary().get(95)};
  mixer[kColorRefSecondary99] = {palette.secondary().get(99)};
  mixer[kColorRefSecondary100] = {palette.secondary().get(100)};

  mixer[kColorRefTertiary0] = {palette.tertiary().get(0)};
  mixer[kColorRefTertiary10] = {palette.tertiary().get(10)};
  mixer[kColorRefTertiary20] = {palette.tertiary().get(20)};
  mixer[kColorRefTertiary30] = {palette.tertiary().get(30)};
  mixer[kColorRefTertiary40] = {palette.tertiary().get(40)};
  mixer[kColorRefTertiary50] = {palette.tertiary().get(50)};
  mixer[kColorRefTertiary60] = {palette.tertiary().get(60)};
  mixer[kColorRefTertiary70] = {palette.tertiary().get(70)};
  mixer[kColorRefTertiary80] = {palette.tertiary().get(80)};
  mixer[kColorRefTertiary90] = {palette.tertiary().get(90)};
  mixer[kColorRefTertiary95] = {palette.tertiary().get(95)};
  mixer[kColorRefTertiary99] = {palette.tertiary().get(99)};
  mixer[kColorRefTertiary100] = {palette.tertiary().get(100)};

  mixer[kColorRefError0] = {palette.error().get(0)};
  mixer[kColorRefError10] = {palette.error().get(10)};
  mixer[kColorRefError20] = {palette.error().get(20)};
  mixer[kColorRefError30] = {palette.error().get(30)};
  mixer[kColorRefError40] = {palette.error().get(40)};
  mixer[kColorRefError50] = {palette.error().get(50)};
  mixer[kColorRefError60] = {palette.error().get(60)};
  mixer[kColorRefError70] = {palette.error().get(70)};
  mixer[kColorRefError80] = {palette.error().get(80)};
  mixer[kColorRefError90] = {palette.error().get(90)};
  mixer[kColorRefError95] = {palette.error().get(95)};
  mixer[kColorRefError99] = {palette.error().get(99)};
  mixer[kColorRefError100] = {palette.error().get(100)};

  mixer[kColorRefNeutral0] = {palette.neutral().get(0)};
  mixer[kColorRefNeutral4] = {palette.neutral().get(4)};
  mixer[kColorRefNeutral6] = {palette.neutral().get(6)};
  mixer[kColorRefNeutral10] = {palette.neutral().get(10)};
  mixer[kColorRefNeutral12] = {palette.neutral().get(12)};
  mixer[kColorRefNeutral17] = {palette.neutral().get(17)};
  mixer[kColorRefNeutral20] = {palette.neutral().get(20)};
  mixer[kColorRefNeutral22] = {palette.neutral().get(22)};
  mixer[kColorRefNeutral24] = {palette.neutral().get(24)};
  mixer[kColorRefNeutral30] = {palette.neutral().get(30)};
  mixer[kColorRefNeutral40] = {palette.neutral().get(40)};
  mixer[kColorRefNeutral50] = {palette.neutral().get(50)};
  mixer[kColorRefNeutral60] = {palette.neutral().get(60)};
  mixer[kColorRefNeutral70] = {palette.neutral().get(70)};
  mixer[kColorRefNeutral80] = {palette.neutral().get(80)};
  mixer[kColorRefNeutral87] = {palette.neutral().get(87)};
  mixer[kColorRefNeutral90] = {palette.neutral().get(90)};
  mixer[kColorRefNeutral92] = {palette.neutral().get(92)};
  mixer[kColorRefNeutral94] = {palette.neutral().get(94)};
  mixer[kColorRefNeutral95] = {palette.neutral().get(95)};
  mixer[kColorRefNeutral96] = {palette.neutral().get(96)};
  mixer[kColorRefNeutral98] = {palette.neutral().get(98)};
  mixer[kColorRefNeutral99] = {palette.neutral().get(99)};
  mixer[kColorRefNeutral100] = {palette.neutral().get(100)};

  mixer[kColorRefNeutralVariant0] = {palette.neutral_variant().get(0)};
  mixer[kColorRefNeutralVariant10] = {palette.neutral_variant().get(10)};
  mixer[kColorRefNeutralVariant20] = {palette.neutral_variant().get(20)};
  mixer[kColorRefNeutralVariant30] = {palette.neutral_variant().get(30)};
  mixer[kColorRefNeutralVariant40] = {palette.neutral_variant().get(40)};
  mixer[kColorRefNeutralVariant50] = {palette.neutral_variant().get(50)};
  mixer[kColorRefNeutralVariant60] = {palette.neutral_variant().get(60)};
  mixer[kColorRefNeutralVariant70] = {palette.neutral_variant().get(70)};
  mixer[kColorRefNeutralVariant80] = {palette.neutral_variant().get(80)};
  mixer[kColorRefNeutralVariant90] = {palette.neutral_variant().get(90)};
  mixer[kColorRefNeutralVariant95] = {palette.neutral_variant().get(95)};
  mixer[kColorRefNeutralVariant99] = {palette.neutral_variant().get(99)};
  mixer[kColorRefNeutralVariant100] = {palette.neutral_variant().get(100)};
}

}  // namespace ui
