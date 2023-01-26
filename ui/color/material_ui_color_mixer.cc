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

  mixer[kColorButtonBackground] = {kColorSysSurface};
  mixer[kColorButtonBackgroundPressed] =
      GetResultingPaintColor({kColorSysStatePressed}, {kColorButtonBackground});
  mixer[kColorButtonBackgroundProminent] = {kColorSysPrimary};
  mixer[kColorButtonBackgroundProminentDisabled] = {GetResultingPaintColor(
      {kColorSysDisabledContainer}, {kColorButtonBackground})};
  mixer[kColorButtonBackgroundProminentFocused] = {GetResultingPaintColor(
      {kColorSysStateFocus}, {kColorButtonBackgroundProminent})};
  mixer[kColorButtonBorder] = {kColorSysOutline};
  mixer[kColorButtonBorderDisabled] = {kColorSysOnSurfaceDisabled};
  mixer[kColorButtonForeground] = {kColorSysOnSurfacePrimary};
  mixer[kColorButtonForegroundChecked] = {kColorButtonForeground};
  mixer[kColorButtonForegroundDisabled] = {kColorSysOnSurfaceDisabled};
  mixer[kColorButtonForegroundProminent] = {kColorSysOnPrimary};
  mixer[kColorButtonForegroundUnchecked] = {kColorSysOnSurfaceVariant};
  mixer[kColorFocusableBorderFocused] = {kColorSysStateFocusRing};
  mixer[kColorFocusableBorderUnfocused] = {kColorSysOutline};
  mixer[kColorFrameActive] = {kColorSysHeader};
  mixer[kColorFrameActiveUnthemed] = {kColorSysHeader};
  mixer[kColorFrameInactive] = {kColorSysHeaderInactive};
  mixer[kColorSliderThumb] = {kColorSysPrimary};
  mixer[kColorSliderThumbMinimal] = {kColorSysSecondary};
  mixer[kColorSliderTrack] = {kColorSysOnPrimary};
  mixer[kColorSliderTrackMinimal] = {kColorSysOnSecondary};
  mixer[kColorToggleButtonShadow] = {kColorSysOutline};
  mixer[kColorToggleButtonThumbOff] = {kColorSysOutline};
  mixer[kColorToggleButtonThumbOn] = {kColorSysOnPrimary};
  mixer[kColorToggleButtonTrackOff] = {kColorSysSurfaceVariant};
  mixer[kColorToggleButtonTrackOn] = {kColorSysPrimary};
  mixer[kColorToggleButtonThumbOnIcon] = {kColorSysOnPrimaryContainer};
}

}  // namespace ui
