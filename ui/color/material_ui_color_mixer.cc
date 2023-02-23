// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/material_ui_color_mixer.h"

#include <utility>

#include "base/logging.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"

namespace ui {

void AddMaterialUiColorMixer(ColorProvider* provider,
                             const ColorProviderManager::Key& key) {
  const bool dark_mode =
      key.color_mode == ColorProviderManager::ColorMode::kDark;
  DVLOG(2) << "Adding MaterialUiColorMixer to ColorProvider for "
           << (dark_mode ? "Dark" : "Light") << " window.";
  ColorMixer& mixer = provider->AddMixer();

  mixer[kColorBadgeBackground] = {kColorSysTonalContainer};
  mixer[kColorBadgeForeground] = {kColorSysOnTonalContainer};
  mixer[kColorButtonBackground] = {kColorSysSurface};
  mixer[kColorButtonBackgroundPressed] =
      GetResultingPaintColor({kColorSysStatePressed}, {kColorButtonBackground});
  mixer[kColorButtonBackgroundProminent] = {kColorSysPrimary};
  mixer[kColorButtonBackgroundProminentDisabled] = {GetResultingPaintColor(
      {kColorSysStateDisabledContainer}, {kColorButtonBackground})};
  mixer[kColorButtonBackgroundProminentFocused] = {GetResultingPaintColor(
      {kColorSysStateFocus}, {kColorButtonBackgroundProminent})};
  mixer[kColorButtonBorder] = {kColorSysOutline};
  mixer[kColorButtonBorderDisabled] = {kColorSysStateDisabled};
  mixer[kColorButtonForeground] = {kColorSysOnSurfacePrimary};
  mixer[kColorButtonForegroundChecked] = {kColorButtonForeground};
  mixer[kColorButtonForegroundDisabled] = {kColorSysStateDisabled};
  mixer[kColorButtonForegroundProminent] = {kColorSysOnPrimary};
  mixer[kColorButtonForegroundUnchecked] = {kColorSysOnSurfaceVariant};
  mixer[kColorCheckboxBackgroundDisabled] = {kColorSysStateDisabledContainer};
  mixer[kColorCheckboxForegroundChecked] = {kColorSysOnSurfacePrimary};
  mixer[kColorCheckboxForegroundDisabled] = {kColorSysStateDisabled};
  mixer[kColorCheckboxForegroundUnchecked] = {kColorSysOnSurfaceVariant};
  mixer[kColorComboboxBackground] = {kColorSysSurface};
  mixer[kColorComboboxBackgroundDisabled] = {GetResultingPaintColor(
      {kColorSysStateDisabledContainer}, {kColorComboboxBackground})};
  mixer[kColorFocusableBorderFocused] = {kColorSysStateFocusRing};
  mixer[kColorFocusableBorderUnfocused] = {kColorSysOutline};
  mixer[kColorFrameActive] = {kColorSysHeader};
  mixer[kColorFrameActiveUnthemed] = {kColorSysHeader};
  mixer[kColorFrameInactive] = {kColorSysHeaderInactive};
  mixer[kColorSliderThumb] = {kColorSysPrimary};
  mixer[kColorSliderThumbMinimal] = {kColorSysSecondary};
  mixer[kColorSliderTrack] = {kColorSysOnPrimary};
  mixer[kColorSliderTrackMinimal] = {kColorSysOnSecondary};
  mixer[kColorTextfieldBackground] = {kColorSysSurface};
  mixer[kColorTextfieldBackgroundDisabled] = {GetResultingPaintColor(
      {kColorSysStateDisabledContainer}, {kColorTextfieldBackground})};
  mixer[kColorTextfieldForeground] = {kColorSysOnSurface};
  mixer[kColorTextfieldForegroundInvalid] = {
      BlendForMinContrast(kColorSysError, kColorTextfieldBackground)};
  mixer[kColorTextfieldForegroundDisabled] = {kColorSysStateDisabled};
  mixer[kColorTextfieldForegroundPlaceholder] = {kColorSysOnSurface};
  mixer[kColorTextfieldInvalidOutline] = {kColorTextfieldForegroundInvalid};
  mixer[kColorToggleButtonShadow] = {kColorSysOutline};
  mixer[kColorToggleButtonThumbOff] = {kColorSysOutline};
  mixer[kColorToggleButtonThumbOffDisabled] = {kColorSysStateDisabled};
  mixer[kColorToggleButtonThumbOn] = {kColorSysOnPrimary};
  mixer[kColorToggleButtonThumbOnDisabled] = {kColorSysSurface};
  mixer[kColorToggleButtonThumbOnIcon] = {kColorSysOnPrimaryContainer};
  mixer[kColorToggleButtonTrackOff] = {kColorSysSurfaceVariant};
  mixer[kColorToggleButtonTrackOn] = {kColorSysPrimary};
  mixer[kColorToggleButtonTrackOnDisabled] = {kColorSysStateDisabledContainer};
}

}  // namespace ui
