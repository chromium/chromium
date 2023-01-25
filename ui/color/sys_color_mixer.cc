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

  // TODO(tluk): Current sys token recipes are still in flux. Audit and update
  // existing definitions once the color spec is final.
  // Core sys tokens.
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
  mixer[kColorSysInversePrimary] = {dark_mode ? kColorRefPrimary40
                                              : kColorRefPrimary80};
  mixer[kColorSysInverseOnSurface] = {dark_mode ? kColorRefNeutral10
                                                : kColorRefNeutral95};

  // Base and surface tokens.
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
  mixer[kColorSysDisabledContainer] = ui::SetAlpha({kColorSysOnSurface}, 0x60);

  // Chrome surfaces.
  mixer[kColorSysHeader] = {dark_mode ? kColorRefNeutral10
                                      : kColorRefPrimary90};
  mixer[kColorSysHeaderInactive] = {dark_mode ? kColorSysSurface1
                                              : kColorSysSurface3};
  mixer[kColorSysOnHeaderDivider] = {dark_mode ? kColorRefNeutral20
                                               : kColorRefPrimary80};
  mixer[kColorSysOnHeaderDividerInactive] = {dark_mode ? kColorRefSecondary30
                                                       : kColorRefNeutral80};
  mixer[kColorSysOnHeaderPrimary] = {dark_mode ? kColorRefPrimary80
                                               : kColorRefPrimary40};
  mixer[kColorSysOnHeaderPrimaryInactive] = {dark_mode ? kColorRefSecondary80
                                                       : kColorRefNeutral40};
  mixer[kColorSysOnHeaderSecondary] = {dark_mode ? kColorRefNeutral80
                                                 : kColorRefNeutral40};
  mixer[kColorSysOnHeaderSecondaryInactive] = {dark_mode ? kColorRefNeutral80
                                                         : kColorRefNeutral40};
  mixer[kColorSysOnHeaderContainer] = {dark_mode ? kColorRefSecondary20
                                                 : kColorRefPrimary95};
  mixer[kColorSysOnHeaderContainerInactive] = {dark_mode ? kColorRefSecondary20
                                                         : kColorRefNeutral99};

  // On-color tokens for any bases.
  mixer[kColorSysOnSurface] = {dark_mode ? kColorRefNeutral90
                                         : kColorRefNeutral10};
  mixer[kColorSysOnSurfaceDisabled] = {
      dark_mode ? SetAlpha({kColorRefNeutral80}, 0x60)
                : SetAlpha({kColorRefNeutral30}, 0x60)};
  mixer[kColorSysOnSurfacePrimary] = {dark_mode ? kColorRefPrimary90
                                                : kColorRefPrimary10};
  mixer[kColorSysOnSurfaceSecondary] = {dark_mode ? kColorRefNeutral80
                                                  : kColorRefNeutral30};
  mixer[kColorSysOnSurfaceSubtle] = {dark_mode ? kColorRefNeutral80
                                               : kColorRefNeutral30};

  // Effect tokens.
  mixer[kColorSysStateHover] = {dark_mode
                                    ? SetAlpha({kColorRefNeutral90}, 0x14)
                                    : SetAlpha({kColorRefNeutral10}, 0x14)};
  mixer[kColorSysStateFocus] = {dark_mode
                                    ? SetAlpha({kColorRefNeutral90}, 0x1E)
                                    : SetAlpha({kColorRefNeutral10}, 0x1E)};
  mixer[kColorSysStatePressed] = {dark_mode
                                      ? SetAlpha({kColorRefNeutral90}, 0x1E)
                                      : SetAlpha({kColorRefNeutral10}, 0x1E)};
  mixer[kColorSysStateDrag] = {dark_mode
                                   ? SetAlpha({kColorRefNeutral90}, 0x29)
                                   : SetAlpha({kColorRefNeutral10}, 0x29)};
  mixer[kColorSysStateFocusRing] = {dark_mode ? kColorRefPrimary80
                                              : kColorRefPrimary40};
}

}  // namespace ui
