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
#include "ui/color/color_transform.h"

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
  mixer[kColorBadgeInCocoaMenuBackground] = {kColorSysPrimary};
  mixer[kColorBadgeInCocoaMenuForeground] = {kColorSysOnPrimary};
  mixer[kColorButtonBackground] = {kColorSysSurface};
  mixer[kColorButtonBackgroundPressed] =
      GetResultingPaintColor({kColorSysStatePressed}, {kColorButtonBackground});
  mixer[kColorButtonBackgroundProminent] = {kColorSysPrimary};
  mixer[kColorButtonBackgroundProminentDisabled] = {GetResultingPaintColor(
      {kColorSysStateDisabledContainer}, {kColorButtonBackground})};
  mixer[kColorButtonBackgroundProminentFocused] = {GetResultingPaintColor(
      {kColorSysStateFocus}, {kColorButtonBackgroundProminent})};
  mixer[kColorButtonBorder] = {kColorSysTonalOutline};
  mixer[kColorButtonBorderDisabled] = {kColorSysStateDisabled};
  mixer[kColorButtonForeground] = {kColorSysPrimary};
  mixer[kColorButtonForegroundDisabled] = {kColorSysStateDisabled};
  mixer[kColorButtonForegroundProminent] = {kColorSysOnPrimary};
  mixer[kColorCheckboxCheck] = {kColorSysOnPrimary};
  mixer[kColorCheckboxCheckDisabled] = {kColorSysStateDisabled};
  mixer[kColorCheckboxContainer] = {kColorSysPrimary};
  mixer[kColorCheckboxContainerDisabled] = {kColorSysStateDisabledContainer};
  mixer[kColorCheckboxOutline] = {kColorSysOutline};
  mixer[kColorCheckboxOutlineDisabled] = {kColorSysStateDisabledContainer};
  mixer[kColorComboboxBackground] = {kColorSysSurface};
  mixer[kColorComboboxBackgroundDisabled] = {GetResultingPaintColor(
      {kColorSysStateDisabledContainer}, {kColorComboboxBackground})};
  mixer[kColorComboboxContainerOutline] = {kColorSysNeutralOutline};
  mixer[kColorComboboxInkDropHovered] = {kColorSysStateHoverOnSubtle};
  mixer[kColorComboboxInkDropRipple] = {kColorSysStateRippleNeutralOnSubtle};
  mixer[kColorFocusableBorderFocused] = {kColorSysStateFocusRing};
  mixer[kColorFocusableBorderUnfocused] = {kColorSysOutline};
  mixer[kColorFrameActive] = {kColorSysHeader};
  mixer[kColorFrameActiveUnthemed] = {kColorSysHeader};
  mixer[kColorFrameInactive] = {kColorSysHeaderInactive};
  mixer[kColorListItemFolderIconBackground] = {kColorSysTonalContainer};
  mixer[kColorListItemFolderIconForeground] = {kColorSysOnTonalContainer};
  mixer[kColorListItemUrlFaviconBackground] = {kColorSysNeutralContainer};
  mixer[kColorMenuButtonBackground] = {kColorSysNeutralContainer};
  mixer[kColorMenuIcon] = {kColorSysOnSurfaceSubtle};
  mixer[kColorMenuItemForegroundSecondary] = {kColorSysOnSurfaceSubtle};
  mixer[kColorMenuItemForeground] = {kColorSysOnSurface};
  mixer[kColorRadioButtonForegroundChecked] = {kColorSysPrimary};
  mixer[kColorRadioButtonForegroundDisabled] = {
      kColorSysStateDisabledContainer};
  mixer[kColorRadioButtonForegroundUnchecked] = {kColorSysOutline};
  mixer[kColorSegmentedButtonBorder] = {kColorSysTonalOutline};
  mixer[kColorSegmentedButtonForegroundChecked] = {kColorSysOnTonalContainer};
  mixer[kColorSegmentedButtonForegroundUnchecked] = {kColorSysOnSurfaceSubtle};
  mixer[kColorSegmentedButtonHover] = {kColorSysStateHoverOnSubtle};
  mixer[kColorSegmentedButtonRipple] = {kColorSysStateRippleNeutralOnSubtle};
  mixer[kColorSegmentedButtonChecked] = {kColorSysTonalContainer};
  mixer[kColorSeparator] = {kColorSysDivider};
  mixer[kColorSliderThumb] = {kColorSysPrimary};
  mixer[kColorSliderThumbMinimal] = {kColorSysSecondary};
  mixer[kColorSliderTrack] = {kColorSysOnPrimary};
  mixer[kColorSliderTrackMinimal] = {kColorSysOnSecondary};
  mixer[kColorSuggestionChipBorder] = {kColorSysTonalOutline};
  mixer[kColorSuggestionChipIcon] = {kColorSysPrimary};
  // TODO(colehorvitz): Rename textfield color IDs to specify which
  // textfield variation they are used for ('filled' or 'stroked').
  mixer[kColorTextfieldBackground] = {kColorSysSurface};
  mixer[kColorTextfieldBackgroundDisabled] = {GetResultingPaintColor(
      {kColorSysStateDisabledContainer}, {kColorTextfieldBackground})};
  mixer[kColorTextfieldFilledUnderline] = {kColorSysOutline};
  mixer[kColorTextfieldFilledUnderlineFocused] = {kColorSysPrimary};
  mixer[kColorTextfieldFilledBackground] = {kColorSysSurfaceVariant};
  mixer[kColorTextfieldFilledForegroundInvalid] = {kColorSysError};
  mixer[kColorTextfieldForeground] = {kColorSysOnSurface};
  mixer[kColorTextfieldForegroundPlaceholderInvalid] = {
      BlendForMinContrast(kColorSysError, kColorTextfieldBackground)};
  mixer[kColorTextfieldForegroundDisabled] = {kColorSysStateDisabled};
  mixer[kColorTextfieldForegroundLabel] = {kColorSysOnSurfaceSubtle};
  mixer[kColorTextfieldForegroundPlaceholder] = {kColorSysOnSurfaceSubtle};
  mixer[kColorTextfieldForegroundIcon] = {kColorSysOnSurfaceSubtle};
  mixer[kColorTextfieldOutline] = {kColorSysNeutralOutline};
  mixer[kColorTextfieldDisabledOutline] = {SK_ColorTRANSPARENT};
  mixer[kColorTextfieldInvalidOutline] = {
      kColorTextfieldForegroundPlaceholderInvalid};
  mixer[kColorTextfieldSelectionBackground] = {kColorSysTonalContainer};
  mixer[kColorToggleButtonHover] = {kColorSysStateHover};
  mixer[kColorToggleButtonPressed] = {kColorSysStatePressed};
  mixer[kColorToggleButtonShadow] = {kColorSysOutline};
  mixer[kColorToggleButtonThumbOff] = {kColorSysOutline};
  mixer[kColorToggleButtonThumbOffDisabled] = {kColorSysStateDisabled};
  mixer[kColorToggleButtonThumbOn] = {kColorSysOnPrimary};
  mixer[kColorToggleButtonThumbOnDisabled] = {kColorSysSurface};
  mixer[kColorToggleButtonThumbOnHover] = {kColorSysPrimaryContainer};
  mixer[kColorToggleButtonTrackOff] = {kColorSysSurfaceVariant};
  mixer[kColorToggleButtonTrackOffDisabled] = {kColorSysSurfaceVariant};
  mixer[kColorToggleButtonTrackOn] = {kColorSysPrimary};
  mixer[kColorToggleButtonTrackOnDisabled] = {kColorSysStateDisabledContainer};
  mixer[kColorToolbarSearchFieldBackground] = {kColorSysBaseContainerElevated};
  mixer[kColorToolbarSearchFieldBackgroundHover] = {
      kColorSysStateHoverDimBlendProtection};
  mixer[kColorToolbarSearchFieldBackgroundPressed] = {
      kColorSysStateRippleNeutralOnSubtle};
  mixer[kColorToolbarSearchFieldForeground] = {kColorSysOnSurface};
  mixer[kColorToolbarSearchFieldForegroundPlaceholder] = {
      kColorSysOnSurfaceSubtle};
  mixer[kColorToolbarSearchFieldIcon] = {kColorSysOnSurfaceSubtle};
}

}  // namespace ui
