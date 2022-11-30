// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/sys_color_mixer.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"

namespace ui {

void AddSysColorMixer(ColorProvider* provider,
                      const ColorProviderManager::Key& key) {
  const bool dark_mode =
      key.color_mode == ColorProviderManager::ColorMode::kDark;
  ColorMixer& mixer = provider->AddMixer();

  mixer[kColorSysPrimary] = {dark_mode ? kColorRefPrimary80
                                       : kColorRefPrimary40};
  mixer[kColorSysOnPrimary] = {dark_mode ? kColorRefPrimary20
                                         : kColorRefPrimary100};
  mixer[kColorSysPrimaryContainer] = {
      dark_mode ? GetResultingPaintColor(SetAlpha({kColorRefPrimary30}, 0x14),
                                         {kColorRefSecondary30})
                : kColorRefPrimary90};
  mixer[kColorSysOnPrimaryContainer] = {dark_mode ? kColorRefPrimary90
                                                  : kColorRefPrimary10};

  mixer[kColorSysSecondary] = {dark_mode ? kColorRefSecondary80
                                         : kColorRefSecondary40};
  mixer[kColorSysOnSecondary] = {dark_mode ? kColorRefSecondary20
                                           : kColorRefSecondary100};
  mixer[kColorSysSecondaryContainer] = {dark_mode ? kColorRefSecondary30
                                                  : kColorRefSecondary90};
  mixer[kColorSysOnSecondaryContainer] = {dark_mode ? kColorRefSecondary90
                                                    : kColorRefSecondary10};

  mixer[kColorSysTertiary] = {dark_mode ? kColorRefTertiary80
                                        : kColorRefTertiary40};
  mixer[kColorSysOnTertiary] = {dark_mode ? kColorRefTertiary20
                                          : kColorRefTertiary100};
  mixer[kColorSysTertiaryContainer] = {dark_mode ? kColorRefTertiary30
                                                 : kColorRefTertiary90};
  mixer[kColorSysOnTertiaryContainer] = {dark_mode ? kColorRefTertiary90
                                                   : kColorRefTertiary10};

  mixer[kColorSysError] = {dark_mode ? kColorRefError80 : kColorRefError40};
  mixer[kColorSysOnError] = {dark_mode ? kColorRefError20 : kColorRefError100};
  mixer[kColorSysErrorContainer] = {dark_mode ? kColorRefError30
                                              : kColorRefError90};
  mixer[kColorSysOnErrorContainer] = {dark_mode ? kColorRefError90
                                                : kColorRefError10};

  mixer[kColorSysSurfaceVariant] = {dark_mode ? kColorRefNeutralVariant30
                                              : kColorRefNeutralVariant90};
  mixer[kColorSysOnSurfaceVariant] = {dark_mode ? kColorRefNeutralVariant80
                                                : kColorRefNeutralVariant30};

  mixer[kColorSysOutline] = {dark_mode ? kColorRefNeutralVariant60
                                       : kColorRefNeutralVariant50};
  mixer[kColorSysScrim] = {dark_mode
                               ? SetAlpha({kColorRefNeutralVariant10}, 0x99)
                               : SetAlpha({kColorRefNeutralVariant60}, 0x99)};
  mixer[kColorSysSeparator] = {dark_mode
                                   ? SetAlpha({kColorRefNeutral90}, 0x23)
                                   : SetAlpha({kColorRefNeutral10}, 0x23)};
  mixer[kColorSysSurface] = {dark_mode ? kColorRefNeutral10
                                       : kColorRefNeutral99};
  mixer[kColorSysSurface1] = {
      dark_mode ? GetResultingPaintColor(SetAlpha({kColorRefPrimary80}, 0x0C),
                                         {kColorRefNeutral10})
                : GetResultingPaintColor(SetAlpha({kColorRefPrimary40}, 0x0C),
                                         {kColorRefNeutral99})};
  mixer[kColorSysSurface2] = {
      dark_mode ? GetResultingPaintColor(SetAlpha({kColorRefPrimary80}, 0x14),
                                         {kColorRefNeutral10})
                : GetResultingPaintColor(SetAlpha({kColorRefPrimary40}, 0x14),
                                         {kColorRefNeutral99})};
  mixer[kColorSysSurface3] = {
      dark_mode ? GetResultingPaintColor(SetAlpha({kColorRefPrimary80}, 0x1C),
                                         {kColorRefNeutral10})
                : GetResultingPaintColor(SetAlpha({kColorRefPrimary40}, 0x1C),
                                         {kColorRefNeutral99})};
  mixer[kColorSysSurface4] = {
      dark_mode ? GetResultingPaintColor(SetAlpha({kColorRefPrimary80}, 0x1E),
                                         {kColorRefNeutral10})
                : GetResultingPaintColor(SetAlpha({kColorRefPrimary40}, 0x1E),
                                         {kColorRefNeutral99})};
  mixer[kColorSysSurface5] = {
      dark_mode ? GetResultingPaintColor(SetAlpha({kColorRefPrimary80}, 0x23),
                                         {kColorRefNeutral10})
                : GetResultingPaintColor(SetAlpha({kColorRefPrimary40}, 0x23),
                                         {kColorRefNeutral99})};
}

}  // namespace ui
