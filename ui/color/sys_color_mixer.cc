// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/sys_color_mixer.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"

namespace ui {

namespace {

// Sys token overrides for the non-baseline themed case.
void AddThemedSysColorOverrides(ColorMixer& mixer,
                                const ColorProviderKey& key) {
  const bool dark_mode = key.color_mode == ColorProviderKey::ColorMode::kDark;

  // Surfaces.
  mixer[kColorSysSurface] = {dark_mode ? kColorRefNeutral10
                                       : kColorRefNeutral99};
  mixer[kColorSysSurfaceNumberedForeground] = {kColorSysPrimary};

  // General.
  mixer[kColorSysOnSurfaceSecondary] = {dark_mode ? kColorRefSecondary80
                                                  : kColorRefSecondary30};
  mixer[kColorSysTonalContainer] = {dark_mode ? kColorRefPrimary30
                                              : kColorRefPrimary90};
  mixer[kColorSysBaseTonalContainer] = {dark_mode ? kColorRefPrimary10
                                                  : kColorRefPrimary90};
  mixer[kColorSysNeutralContainer] = {dark_mode ? kColorRefNeutralVariant15
                                                : kColorRefNeutral94};
  mixer[kColorSysDivider] = {dark_mode ? kColorRefSecondary35
                                       : kColorRefPrimary90};

  // Chrome surfaces.
  mixer[kColorSysBase] = {dark_mode ? kColorRefSecondary25
                                    : kColorRefNeutral98};
  mixer[kColorSysBaseContainer] = {dark_mode ? kColorRefSecondary15
                                             : kColorSysSurface4};
  mixer[kColorSysBaseContainerElevated] = {dark_mode ? kColorRefSecondary25
                                                     : kColorRefNeutral98};

  mixer[kColorSysHeader] = {dark_mode ? kColorRefSecondary12
                                      : kColorRefSecondary90};
  mixer[kColorSysHeaderContainer] = {dark_mode ? kColorRefSecondary25
                                               : kColorRefPrimary95};
  mixer[kColorSysHeaderContainerInactive] = {dark_mode ? kColorRefNeutral25
                                                       : kColorRefNeutral99};
  mixer[kColorSysOnHeaderDivider] = {dark_mode ? kColorRefSecondary25
                                               : kColorRefPrimary80};

  // States.
  mixer[kColorSysStateInactiveRing] = {
      dark_mode ? SetAlpha({kColorRefPrimary70}, 0x8C)
                : SetAlpha({kColorRefPrimary20}, 0x8C)};
  mixer[kColorSysStateOnHeaderHover] = {dark_mode ? kColorRefPrimary90
                                                  : kColorRefPrimary20};
  mixer[kColorSysStateHeaderHover] = {dark_mode ? kColorRefPrimary30
                                                : kColorRefPrimary80};

  // Experimentation.
  mixer[kColorSysOmniboxContainer] = {dark_mode ? kColorRefSecondary15
                                                : kColorSysSurface4};
}

// Overrides for the grayscale baseline case.
// TODO(tluk): This can probably be migrated to the kMonochrome SchemeVariant
// when available.
void AddGrayscaleSysColorOverrides(ColorMixer& mixer,
                                   const ColorProviderKey& key) {
  const bool dark_mode = key.color_mode == ColorProviderKey::ColorMode::kDark;

  // General
  mixer[kColorSysOnSurfacePrimary] = {dark_mode ? kColorRefNeutral90
                                                : kColorRefNeutral10};
  mixer[kColorSysDivider] = {dark_mode ? kColorRefNeutral40
                                       : kColorRefNeutral90};

  // Chrome surfaces.
  mixer[kColorSysHeader] = {dark_mode ? kColorRefNeutral12
                                      : kColorRefNeutral90};
  mixer[kColorSysHeaderInactive] = {
      dark_mode ? AlphaBlend({kColorSysHeader}, {kColorRefNeutral25}, 0x99)
                : AlphaBlend({kColorSysHeader}, {kColorRefNeutral98}, 0x48)};
  mixer[kColorSysHeaderContainer] = {dark_mode ? kColorRefNeutral25
                                               : kColorRefNeutral95};
  mixer[kColorSysOnHeaderDivider] = {dark_mode ? kColorRefNeutral25
                                               : kColorRefNeutral80};
  mixer[kColorSysOnHeaderPrimary] = {dark_mode ? kColorRefNeutral80
                                               : kColorRefNeutral40};

  // States.
  mixer[kColorSysStateInactiveRing] = {
      dark_mode ? SetAlpha({kColorRefNeutral70}, 0x8C)
                : SetAlpha({kColorRefNeutral20}, 0x8C)};
  mixer[kColorSysStateOnHeaderHover] = {dark_mode ? kColorRefNeutral90
                                                  : kColorRefNeutral20};
  mixer[kColorSysStateHeaderHover] = {dark_mode ? kColorRefNeutral30
                                                : kColorRefNeutral80};

  // Experimentation.
  mixer[kColorSysOmniboxContainer] = {dark_mode ? kColorRefNeutral15
                                                : kColorRefNeutral94};
}

}  // namespace

void AddSysColorMixer(ColorProvider* provider, const ColorProviderKey& key) {
  const bool dark_mode = key.color_mode == ColorProviderKey::ColorMode::kDark;
  ColorMixer& mixer = provider->AddMixer();

  // Primary.
  mixer[kColorSysPrimary] = {dark_mode ? kColorRefPrimary80
                                       : kColorRefPrimary40};
  mixer[kColorSysOnPrimary] = {dark_mode ? kColorRefPrimary20
                                         : kColorRefPrimary100};
  mixer[kColorSysPrimaryContainer] = {dark_mode ? kColorRefPrimary30
                                                : kColorRefPrimary90};
  mixer[kColorSysOnPrimaryContainer] = {dark_mode ? kColorRefPrimary90
                                                  : kColorRefPrimary10};

  // Secondary.
  mixer[kColorSysSecondary] = {dark_mode ? kColorRefSecondary80
                                         : kColorRefSecondary40};
  mixer[kColorSysOnSecondary] = {dark_mode ? kColorRefSecondary20
                                           : kColorRefSecondary100};
  mixer[kColorSysSecondaryContainer] = {dark_mode ? kColorRefSecondary30
                                                  : kColorRefSecondary90};
  mixer[kColorSysOnSecondaryContainer] = {dark_mode ? kColorRefSecondary90
                                                    : kColorRefSecondary10};
  // Tertiary.
  mixer[kColorSysTertiary] = {dark_mode ? kColorRefTertiary80
                                        : kColorRefTertiary40};
  mixer[kColorSysOnTertiary] = {dark_mode ? kColorRefTertiary20
                                          : kColorRefTertiary100};
  mixer[kColorSysTertiaryContainer] = {dark_mode ? kColorRefTertiary30
                                                 : kColorRefTertiary90};
  mixer[kColorSysOnTertiaryContainer] = {dark_mode ? kColorRefTertiary90
                                                   : kColorRefTertiary10};

  // Error.
  mixer[kColorSysError] = {dark_mode ? kColorRefError80 : kColorRefError40};
  mixer[kColorSysOnError] = {dark_mode ? kColorRefError20 : kColorRefError100};
  mixer[kColorSysErrorContainer] = {dark_mode ? kColorRefError30
                                              : kColorRefError90};
  mixer[kColorSysOnErrorContainer] = {dark_mode ? kColorRefError90
                                                : kColorRefError10};
  // Neutral.
  mixer[kColorSysOnSurface] = {dark_mode ? kColorRefNeutral90
                                         : kColorRefNeutral10};
  mixer[kColorSysOnSurfaceVariant] = {dark_mode ? kColorRefNeutralVariant80
                                                : kColorRefNeutralVariant30};
  mixer[kColorSysOutline] = {dark_mode ? kColorRefNeutralVariant60
                                       : kColorRefNeutralVariant50};
  mixer[kColorSysSurfaceVariant] = {dark_mode ? kColorRefNeutralVariant30
                                              : kColorRefNeutralVariant90};

  // Constant.
  mixer[kColorSysBlack] = {kColorRefNeutral0};
  mixer[kColorSysWhite] = {kColorRefNeutral100};

  // Inverse.
  mixer[kColorSysInversePrimary] = {dark_mode ? kColorRefPrimary40
                                              : kColorRefPrimary80};
  mixer[kColorSysInverseOnSurface] = {dark_mode ? kColorRefNeutral10
                                                : kColorRefNeutral95};
  mixer[kColorSysInverseSurface] = {dark_mode ? kColorRefNeutral90
                                              : kColorRefNeutral20};
  mixer[kColorSysInverseSurfacePrimary] = {dark_mode ? kColorRefPrimary95
                                                     : kColorRefPrimary20};

  // Surfaces.
  mixer[kColorSysSurface] = {dark_mode ? kColorRefNeutral10
                                       : kColorRefNeutral100};
  mixer[kColorSysSurfaceNumberedForeground] = {
      dark_mode ? SkColorSetRGB(0xD1, 0xE1, 0xFF)
                : SkColorSetRGB(0x69, 0x91, 0xD6)};
  mixer[kColorSysSurface1] = AlphaBlend({kColorSysSurfaceNumberedForeground},
                                        {kColorSysSurface}, 0x0C);
  mixer[kColorSysSurface2] = AlphaBlend({kColorSysSurfaceNumberedForeground},
                                        {kColorSysSurface}, 0x14);
  mixer[kColorSysSurface3] = AlphaBlend({kColorSysSurfaceNumberedForeground},
                                        {kColorSysSurface}, 0x1C);
  mixer[kColorSysSurface4] = AlphaBlend({kColorSysSurfaceNumberedForeground},
                                        {kColorSysSurface}, 0x1E);
  mixer[kColorSysSurface5] = AlphaBlend({kColorSysSurfaceNumberedForeground},
                                        {kColorSysSurface}, 0x23);

  // General.
  mixer[kColorSysOnSurfaceSecondary] = {dark_mode ? kColorRefNeutral80
                                                  : kColorRefNeutral30};
  mixer[kColorSysOnSurfaceSubtle] = {dark_mode ? kColorRefNeutral80
                                               : kColorRefNeutral30};
  mixer[kColorSysOnSurfacePrimary] = {dark_mode ? kColorRefPrimary90
                                                : kColorRefPrimary10};
  mixer[kColorSysOnSurfacePrimaryInactive] = {dark_mode ? kColorRefNeutral90
                                                        : kColorRefNeutral10};

  mixer[kColorSysTonalContainer] = {dark_mode ? kColorRefSecondary30
                                              : kColorRefPrimary90};
  mixer[kColorSysOnTonalContainer] = {dark_mode ? kColorRefSecondary90
                                                : kColorRefPrimary10};
  mixer[kColorSysBaseTonalContainer] = {dark_mode ? kColorRefSecondary10
                                                  : kColorRefPrimary90};
  mixer[kColorSysOnBaseTonalContainer] = {dark_mode ? kColorRefSecondary90
                                                    : kColorRefPrimary10};
  mixer[kColorSysTonalOutline] = {dark_mode ? kColorRefSecondary50
                                            : kColorRefPrimary80};
  mixer[kColorSysNeutralOutline] = {dark_mode ? kColorRefNeutral50
                                              : kColorRefNeutral80};
  mixer[kColorSysNeutralContainer] = {dark_mode ? kColorRefNeutral15
                                                : kColorRefNeutral95};
  mixer[kColorSysDivider] = {dark_mode ? kColorRefNeutral40
                                       : kColorRefPrimary90};

  // Chrome surfaces.
  mixer[kColorSysBase] = {dark_mode ? kColorRefNeutral25 : kColorRefNeutral100};
  mixer[kColorSysBaseContainer] = {dark_mode ? kColorRefNeutral15
                                             : kColorSysSurface4};
  mixer[kColorSysBaseContainerElevated] = {dark_mode ? kColorRefNeutral25
                                                     : kColorRefNeutral100};

  mixer[kColorSysHeader] = {dark_mode ? kColorRefNeutral12
                                      : kColorRefPrimary90};
  mixer[kColorSysHeaderInactive] = {
      dark_mode
          ? AlphaBlend({kColorSysHeader}, {kColorRefNeutral25}, 0x99)
          : AlphaBlend({kColorSysHeader}, {kColorSysSurfaceVariant}, 0x48)};
  mixer[kColorSysHeaderContainer] = {dark_mode ? kColorRefNeutral25
                                               : kColorRefPrimary95};
  mixer[kColorSysHeaderContainerInactive] = {dark_mode ? kColorRefNeutral25
                                                       : kColorRefNeutral100};
  mixer[kColorSysOnHeaderDivider] = {dark_mode ? kColorRefNeutral25
                                               : kColorRefPrimary80};
  mixer[kColorSysOnHeaderDividerInactive] = {dark_mode ? kColorRefNeutral25
                                                       : kColorRefNeutral80};
  mixer[kColorSysOnHeaderPrimary] = {dark_mode ? kColorRefPrimary80
                                               : kColorRefPrimary40};
  mixer[kColorSysOnHeaderPrimaryInactive] = {dark_mode ? kColorRefNeutral80
                                                       : kColorRefNeutral40};

  // States.
  mixer[kColorSysStateHoverOnProminent] = {
      dark_mode ? SetAlpha({kColorRefNeutral10}, 0x0F)
                : SetAlpha({kColorRefNeutral99}, 0x1A)};
  mixer[kColorSysStateHoverOnSubtle] = {
      dark_mode ? SetAlpha({kColorRefNeutral99}, 0x1A)
                : SetAlpha({kColorRefNeutral10}, 0x0F)};
  mixer[kColorSysStateRippleNeutralOnProminent] = {
      dark_mode ? SetAlpha({kColorRefNeutral10}, 0x1F)
                : SetAlpha({kColorRefNeutral99}, 0x29)};
  mixer[kColorSysStateRippleNeutralOnSubtle] = {
      dark_mode ? SetAlpha({kColorRefNeutral99}, 0x29)
                : SetAlpha({kColorRefNeutral10}, 0x14)};
  mixer[kColorSysStateRipplePrimary] = {
      dark_mode ? SetAlpha({kColorRefPrimary60}, 0x52)
                : SetAlpha({kColorRefPrimary70}, 0x52)};
  mixer[kColorSysStateFocusRing] = {dark_mode ? kColorRefPrimary80
                                              : kColorRefPrimary40};
  mixer[kColorSysStateTextHighlight] = {dark_mode ? kColorRefPrimary80
                                                  : kColorRefPrimary40};
  mixer[kColorSysStateOnTextHighlight] = {dark_mode ? kColorRefNeutral0
                                                    : kColorRefNeutral100};
  mixer[kColorSysStateFocusHighlight] = {
      dark_mode ? SetAlpha({kColorRefNeutral99}, 0x1A)
                : SetAlpha({kColorRefNeutral10}, 0x0F)};
  mixer[kColorSysStateDisabled] = dark_mode
                                      ? SetAlpha({kColorRefNeutral90}, 0x60)
                                      : SetAlpha({kColorRefNeutral10}, 0x60);
  mixer[kColorSysStateDisabledContainer] =
      dark_mode ? SetAlpha({kColorRefNeutral90}, 0x1E)
                : SetAlpha({kColorRefNeutral10}, 0x1E);
  mixer[kColorSysStateHoverDimBlendProtection] = {
      dark_mode ? SetAlpha({kColorRefNeutral99}, 0x1A)
                : SetAlpha({kColorRefPrimary20}, 0x2E)};
  mixer[kColorSysStateHoverBrightBlendProtection] = {
      dark_mode ? SetAlpha({kColorRefNeutral99}, 0x29)
                : SetAlpha({kColorRefNeutral10}, 0x0F)};
  mixer[kColorSysStateInactiveRing] = {
      dark_mode ? SetAlpha({kColorRefNeutral70}, 0x8C)
                : SetAlpha({kColorRefPrimary20}, 0x8C)};
  mixer[kColorSysStateScrim] = {SetAlpha({kColorRefNeutral0}, 0x99)};
  mixer[kColorSysStateOnHeaderHover] = {dark_mode ? kColorRefSecondary90
                                                  : kColorRefPrimary20};
  mixer[kColorSysStateHeaderHover] = {dark_mode ? kColorRefSecondary30
                                                : kColorRefPrimary80};
  mixer[kColorSysStateHeaderHoverInactive] = {kColorSysStateHoverOnSubtle};
  mixer[kColorSysStateHeaderSelect] = {SetAlpha({kColorSysBase}, 0x9A)};

  // Effects.
  mixer[kColorSysShadow] = {kColorRefNeutral0};
  mixer[kColorSysGradientPrimary] = {dark_mode ? kColorRefPrimary30
                                               : kColorRefPrimary90};
  mixer[kColorSysGradientTertiary] = {dark_mode ? kColorRefTertiary30
                                                : kColorRefTertiary95};

  // AI.
  mixer[kColorSysAiIllustrationShapeSurface1] = {
      dark_mode ? ui::kColorRefPrimary40 : ui::kColorRefPrimary70};
  mixer[kColorSysAiIllustrationShapeSurface2] = {
      dark_mode ? ui::kColorRefPrimary50 : ui::kColorRefPrimary95};
  mixer[kColorSysAiIllustrationShapeSurfaceGradientStart] = {
      dark_mode ? ui::kColorRefPrimary30 : ui::kColorRefSecondary90};
  mixer[kColorSysAiIllustrationShapeSurfaceGradientEnd] = {
      dark_mode ? ui::kColorRefPrimary40 : ui::kColorRefPrimary80};

  // Experimentation.
  mixer[kColorSysOmniboxContainer] = {dark_mode ? kColorRefNeutral15
                                                : kColorSysSurface4};

  // Deprecated.
  // TODO(crbug.com/350783235): Remove remaining uses of these deprecated sys
  // tokens.
  mixer[kColorSysStateHover] = dark_mode ? SetAlpha({kColorRefNeutral90}, 0x14)
                                         : SetAlpha({kColorRefNeutral10}, 0x14);
  mixer[kColorSysStateFocus] = dark_mode ? SetAlpha({kColorRefNeutral90}, 0x1E)
                                         : SetAlpha({kColorRefNeutral10}, 0x1E);
  mixer[kColorSysStatePressed] = dark_mode
                                     ? SetAlpha({kColorRefNeutral90}, 0x1E)
                                     : SetAlpha({kColorRefNeutral10}, 0x1E);

  // If grayscale is specified the design intention is to apply the grayscale
  // overrides over the baseline palette.
  if (key.user_color_source == ColorProviderKey::UserColorSource::kGrayscale) {
    AddGrayscaleSysColorOverrides(mixer, key);
  } else if (key.user_color_source ==
                 ColorProviderKey::UserColorSource::kAccent &&
             key.user_color.has_value()) {
    AddThemedSysColorOverrides(mixer, key);
  }
}

}  // namespace ui
