// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/ref_color_mixer.h"

#include "base/logging.h"
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

  // TODO(skau): Replace with official implementation when available.
  ToneMap tone_map = GetTempPalette(seed_color);

  ColorMixer& mixer = provider->AddMixer();
  mixer[kColorRefPrimary0] = {tone_map.primary[Luma::k0]};
  mixer[kColorRefPrimary10] = {tone_map.primary[Luma::k10]};
  mixer[kColorRefPrimary20] = {tone_map.primary[Luma::k20]};
  mixer[kColorRefPrimary30] = {tone_map.primary[Luma::k30]};
  mixer[kColorRefPrimary40] = {tone_map.primary[Luma::k40]};
  mixer[kColorRefPrimary50] = {tone_map.primary[Luma::k50]};
  mixer[kColorRefPrimary60] = {tone_map.primary[Luma::k60]};
  mixer[kColorRefPrimary70] = {tone_map.primary[Luma::k70]};
  mixer[kColorRefPrimary80] = {tone_map.primary[Luma::k80]};
  mixer[kColorRefPrimary90] = {tone_map.primary[Luma::k90]};
  mixer[kColorRefPrimary95] = {tone_map.primary[Luma::k95]};
  mixer[kColorRefPrimary99] = {tone_map.primary[Luma::k99]};
  mixer[kColorRefPrimary100] = {tone_map.primary[Luma::k100]};

  mixer[kColorRefSecondary0] = {tone_map.secondary[Luma::k0]};
  mixer[kColorRefSecondary10] = {tone_map.secondary[Luma::k10]};
  mixer[kColorRefSecondary20] = {tone_map.secondary[Luma::k20]};
  mixer[kColorRefSecondary30] = {tone_map.secondary[Luma::k30]};
  mixer[kColorRefSecondary40] = {tone_map.secondary[Luma::k40]};
  mixer[kColorRefSecondary50] = {tone_map.secondary[Luma::k50]};
  mixer[kColorRefSecondary60] = {tone_map.secondary[Luma::k60]};
  mixer[kColorRefSecondary70] = {tone_map.secondary[Luma::k70]};
  mixer[kColorRefSecondary80] = {tone_map.secondary[Luma::k80]};
  mixer[kColorRefSecondary90] = {tone_map.secondary[Luma::k90]};
  mixer[kColorRefSecondary95] = {tone_map.secondary[Luma::k95]};
  mixer[kColorRefSecondary99] = {tone_map.secondary[Luma::k99]};
  mixer[kColorRefSecondary100] = {tone_map.secondary[Luma::k100]};

  mixer[kColorRefTertiary0] = {tone_map.tertiary[Luma::k0]};
  mixer[kColorRefTertiary10] = {tone_map.tertiary[Luma::k10]};
  mixer[kColorRefTertiary20] = {tone_map.tertiary[Luma::k20]};
  mixer[kColorRefTertiary30] = {tone_map.tertiary[Luma::k30]};
  mixer[kColorRefTertiary40] = {tone_map.tertiary[Luma::k40]};
  mixer[kColorRefTertiary50] = {tone_map.tertiary[Luma::k50]};
  mixer[kColorRefTertiary60] = {tone_map.tertiary[Luma::k60]};
  mixer[kColorRefTertiary70] = {tone_map.tertiary[Luma::k70]};
  mixer[kColorRefTertiary80] = {tone_map.tertiary[Luma::k80]};
  mixer[kColorRefTertiary90] = {tone_map.tertiary[Luma::k90]};
  mixer[kColorRefTertiary95] = {tone_map.tertiary[Luma::k95]};
  mixer[kColorRefTertiary99] = {tone_map.tertiary[Luma::k99]};
  mixer[kColorRefTertiary100] = {tone_map.tertiary[Luma::k100]};

  mixer[kColorRefError0] = {tone_map.error[Luma::k0]};
  mixer[kColorRefError10] = {tone_map.error[Luma::k10]};
  mixer[kColorRefError20] = {tone_map.error[Luma::k20]};
  mixer[kColorRefError30] = {tone_map.error[Luma::k30]};
  mixer[kColorRefError40] = {tone_map.error[Luma::k40]};
  mixer[kColorRefError50] = {tone_map.error[Luma::k50]};
  mixer[kColorRefError60] = {tone_map.error[Luma::k60]};
  mixer[kColorRefError70] = {tone_map.error[Luma::k70]};
  mixer[kColorRefError80] = {tone_map.error[Luma::k80]};
  mixer[kColorRefError90] = {tone_map.error[Luma::k90]};
  mixer[kColorRefError95] = {tone_map.error[Luma::k95]};
  mixer[kColorRefError99] = {tone_map.error[Luma::k99]};
  mixer[kColorRefError100] = {tone_map.error[Luma::k100]};

  mixer[kColorRefNeutral0] = {tone_map.neutral1[Luma::k0]};
  mixer[kColorRefNeutral10] = {tone_map.neutral1[Luma::k10]};
  mixer[kColorRefNeutral20] = {tone_map.neutral1[Luma::k20]};
  mixer[kColorRefNeutral30] = {tone_map.neutral1[Luma::k30]};
  mixer[kColorRefNeutral40] = {tone_map.neutral1[Luma::k40]};
  mixer[kColorRefNeutral50] = {tone_map.neutral1[Luma::k50]};
  mixer[kColorRefNeutral60] = {tone_map.neutral1[Luma::k60]};
  mixer[kColorRefNeutral70] = {tone_map.neutral1[Luma::k70]};
  mixer[kColorRefNeutral80] = {tone_map.neutral1[Luma::k80]};
  mixer[kColorRefNeutral90] = {tone_map.neutral1[Luma::k90]};
  mixer[kColorRefNeutral95] = {tone_map.neutral1[Luma::k95]};
  mixer[kColorRefNeutral99] = {tone_map.neutral1[Luma::k99]};
  mixer[kColorRefNeutral100] = {tone_map.neutral1[Luma::k100]};

  mixer[kColorRefNeutralVariant0] = {tone_map.neutral2[Luma::k0]};
  mixer[kColorRefNeutralVariant10] = {tone_map.neutral2[Luma::k10]};
  mixer[kColorRefNeutralVariant20] = {tone_map.neutral2[Luma::k20]};
  mixer[kColorRefNeutralVariant30] = {tone_map.neutral2[Luma::k30]};
  mixer[kColorRefNeutralVariant40] = {tone_map.neutral2[Luma::k40]};
  mixer[kColorRefNeutralVariant50] = {tone_map.neutral2[Luma::k50]};
  mixer[kColorRefNeutralVariant60] = {tone_map.neutral2[Luma::k60]};
  mixer[kColorRefNeutralVariant70] = {tone_map.neutral2[Luma::k70]};
  mixer[kColorRefNeutralVariant80] = {tone_map.neutral2[Luma::k80]};
  mixer[kColorRefNeutralVariant90] = {tone_map.neutral2[Luma::k90]};
  mixer[kColorRefNeutralVariant95] = {tone_map.neutral2[Luma::k95]};
  mixer[kColorRefNeutralVariant99] = {tone_map.neutral2[Luma::k99]};
  mixer[kColorRefNeutralVariant100] = {tone_map.neutral2[Luma::k100]};
}

}  // namespace ui
