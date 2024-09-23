// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/material_ui_color_mixer.h"

#include <utility>

#include "base/logging.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"

namespace ui {
// This aligns with GM2 default InkDropHighlight::visible_opacity_.
constexpr SkAlpha kAttentionHighlightAlpha = 0.128 * 255;

void AddMaterialUiColorMixer(ColorProvider* provider,
                             const ColorProviderKey& key) {
  const bool dark_mode = key.color_mode == ColorProviderKey::ColorMode::kDark;
  DVLOG(2) << "Adding MaterialUiColorMixer to ColorProvider for "
           << (dark_mode ? "Dark" : "Light") << " window.";
  ColorMixer& mixer = provider->AddMixer();

  mixer[kColorAvatarIconIncognito] = {kColorSysOnSurfaceVariant};
  mixer[kColorAppMenuProfileRowBackground] = {kColorSysSurface5};

  mixer[kColorAppMenuProfileRowChipBackground] = {kColorSysTonalContainer};
  mixer[kColorAppMenuProfileRowChipHovered] = {
      ui::GetResultingPaintColor(kColorAppMenuRowBackgroundHovered,
                                 kColorAppMenuProfileRowChipBackground)};
  mixer[kColorAppMenuRowBackgroundHovered] = {kColorSysStateHoverOnSubtle};
  mixer[kColorAppMenuUpgradeRowBackground] = {kColorSysTonalContainer};
  mixer[kColorAppMenuUpgradeRowSubstringForeground] = {
      kColorSysOnTonalContainer};
  mixer[kColorAvatarIconGuest] = {kColorSysOnSurfaceSecondary};
  mixer[kColorBadgeBackground] = {kColorSysTonalContainer};
  mixer[kColorBadgeForeground] = {kColorSysOnTonalContainer};
  mixer[kColorBadgeInCocoaMenuBackground] = {kColorSysPrimary};
  mixer[kColorBadgeInCocoaMenuForeground] = {kColorSysOnPrimary};
  mixer[kColorBubbleBackground] = {kColorSysSurface};
  mixer[kColorBubbleFooterBackground] = {kColorSysNeutralContainer};
  mixer[kColorButtonBackground] = {kColorSysSurface};
  mixer[kColorButtonBackgroundPressed] =
      GetResultingPaintColor({kColorSysStatePressed}, {kColorButtonBackground});
  mixer[kColorButtonBackgroundProminent] = {kColorSysPrimary};
  mixer[kColorButtonBackgroundProminentDisabled] = {GetResultingPaintColor(
      {kColorSysStateDisabledContainer}, {kColorButtonBackground})};
  mixer[kColorButtonBackgroundProminentFocused] = {GetResultingPaintColor(
      {kColorSysStateFocus}, {kColorButtonBackgroundProminent})};
  mixer[kColorButtonBorder] = {kColorSysTonalOutline};
  mixer[kColorButtonBorderDisabled] = {kColorSysStateDisabledContainer};
  mixer[kColorButtonFeatureAttentionHighlight] =
      SetAlpha({kColorSysPrimary}, kAttentionHighlightAlpha);
  mixer[kColorButtonForeground] = {kColorSysPrimary};
  mixer[kColorButtonForegroundDisabled] = {kColorSysStateDisabled};
  mixer[kColorButtonForegroundProminent] = {kColorSysOnPrimary};
  mixer[kColorCheckboxCheck] = {kColorSysOnPrimary};
  mixer[kColorCheckboxCheckDisabled] = {kColorSysStateDisabled};
  mixer[kColorCheckboxContainer] = {kColorSysPrimary};
  mixer[kColorCheckboxContainerDisabled] = {kColorSysStateDisabledContainer};
  mixer[kColorCheckboxOutline] = {kColorSysOutline};
  mixer[kColorCheckboxOutlineDisabled] = {kColorSysStateDisabledContainer};
  mixer[kColorChipBackgroundHover] = {kColorSysStateHoverOnSubtle};
  mixer[kColorChipBackgroundSelected] = {kColorSysTonalContainer};
  mixer[kColorChipBorder] = {kColorSysTonalOutline};
  mixer[kColorChipForeground] = {kColorSysOnSurface};
  mixer[kColorChipForegroundSelected] = {kColorSysOnTonalContainer};
  mixer[kColorChipIcon] = {kColorSysPrimary};
  mixer[kColorChipIconSelected] = {kColorSysOnTonalContainer};
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
  mixer[kColorFrameCaptionButtonUnfocused] = {dark_mode ? kColorRefSecondary100
                                                        : kColorRefSecondary0};
  mixer[kColorFrameInactive] = {kColorSysHeaderInactive};
  mixer[kColorIcon] = {kColorSysOnSurfaceSubtle};
  mixer[kColorHistoryClustersSidePanelDivider] = {kColorSysDivider};
  mixer[kColorHistoryClustersSidePanelDialogBackground] = {kColorSysSurface};
  mixer[kColorHistoryClustersSidePanelDialogDivider] = {
      kColorSysNeutralOutline};
  mixer[kColorHistoryClustersSidePanelDialogPrimaryForeground] = {
      kColorSysOnSurface};
  mixer[kColorHistoryClustersSidePanelDialogSecondaryForeground] = {
      kColorSysOnSurfaceSubtle};
  mixer[kColorHistoryClustersSidePanelCardSecondaryForeground] = {
      kColorSysOnSurfaceSubtle};
  mixer[kColorLabelSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorLinkForeground] = {kColorLinkForegroundDefault};
  mixer[kColorLinkForegroundDefault] = {kColorSysPrimary};
  mixer[kColorLinkForegroundOnBubbleFooter] = {kColorLinkForegroundDefault};
  mixer[kColorLinkForegroundPressed] = {kColorLinkForegroundPressedDefault};
  mixer[kColorLinkForegroundPressedOnBubbleFooter] = {
      kColorLinkForegroundPressedDefault};
  mixer[kColorListItemFolderIconBackground] = {kColorSysTonalContainer};
  mixer[kColorListItemFolderIconForeground] = {kColorSysOnTonalContainer};
  mixer[kColorListItemUrlFaviconBackground] = {kColorSysNeutralContainer};
  mixer[kColorLoadingGradientBorder] = {kColorSysTonalContainer};
  mixer[kColorLoadingGradientEnd] = {kColorSysGradientTertiary};
  mixer[kColorLoadingGradientMiddle] = {kColorSysGradientPrimary};
  mixer[kColorLoadingGradientStart] = {SK_ColorTRANSPARENT};
  mixer[kColorMenuButtonBackground] = {kColorSysNeutralContainer};
  mixer[kColorMenuButtonBackgroundSelected] = {GetResultingPaintColor(
      {kColorSysStateHoverOnSubtle}, {kColorMenuButtonBackground})};
  mixer[kColorMenuIcon] = {kColorSysOnSurfaceSubtle};
  mixer[kColorMenuIconDisabled] = {kColorSysStateDisabled};
  mixer[kColorMenuIconOnEmphasizedBackground] = {kColorSysOnTonalContainer};
  mixer[kColorMenuItemForegroundSecondary] = {kColorSysOnSurfaceSubtle};
  mixer[kColorMenuItemForeground] = {kColorSysOnSurface};
  mixer[kColorMenuSelectionBackground] = {GetResultingPaintColor(
      {kColorSysStateHoverOnSubtle}, {kColorMenuBackground})};
  mixer[kColorPrimaryBackground] = {kColorSysSurface};
  mixer[kColorPrimaryForeground] = {kColorSysOnSurface};
  mixer[kColorProgressBar] = {ui::kColorSysPrimary};
  mixer[kColorProgressBarBackground] = {ui::kColorSysNeutralOutline};
  mixer[kColorProgressBarPaused] = {ui::kColorSysStateDisabled};
  mixer[kColorRadioButtonForegroundChecked] = {kColorSysPrimary};
  mixer[kColorRadioButtonForegroundDisabled] = {
      kColorSysStateDisabledContainer};
  mixer[kColorRadioButtonForegroundUnchecked] = {kColorSysOutline};
  mixer[kColorSecondaryForeground] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorSegmentedButtonBorder] = {kColorSysTonalOutline};
  mixer[kColorSegmentedButtonForegroundChecked] = {kColorSysOnPrimary};
  mixer[kColorSegmentedButtonForegroundUnchecked] = {kColorSysOnSurfaceSubtle};
  mixer[kColorSegmentedButtonHover] = {kColorSysStateHoverOnSubtle};
  mixer[kColorSegmentedButtonRipple] = {kColorSysStateRippleNeutralOnSubtle};
  mixer[kColorSegmentedButtonChecked] = {kColorSysPrimary};
  mixer[kColorSeparator] = {kColorSysDivider};
  mixer[kColorSidePanelComboboxBackground] = {kColorSysBaseContainer};
  mixer[kColorSliderThumb] = {kColorSysPrimary};
  mixer[kColorSliderThumbMinimal] = {kColorSysSecondary};
  mixer[kColorSliderTrack] = {kColorSysOnPrimary};
  mixer[kColorSliderTrackMinimal] = {kColorSysOnSecondary};
  mixer[kColorSuggestionChipBorder] = {kColorSysTonalOutline};
  mixer[kColorSuggestionChipIcon] = {kColorSysPrimary};
  mixer[kColorTabBorderSelected] = {kColorSysPrimary};
  mixer[kColorTabForeground] = {kColorSecondaryForeground};
  mixer[kColorTabForegroundSelected] = {kColorSysPrimary};
  mixer[kColorTableBackground] = {kColorPrimaryBackground};
  mixer[kColorTableBackgroundAlternate] = {kColorTableBackground};
  mixer[kColorTableBackgroundSelectedFocused] = {kColorSysTonalContainer};
  mixer[kColorTableBackgroundSelectedUnfocused] = {
      kColorTableBackgroundSelectedFocused};
  mixer[kColorTableForeground] = {kColorPrimaryForeground};
  mixer[kColorTableForegroundSelectedFocused] = {kColorTableForeground};
  mixer[kColorTableForegroundSelectedUnfocused] = {
      kColorTableForegroundSelectedFocused};
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
  mixer[kColorTextfieldHover] = {kColorSysStateHoverOnSubtle};
  mixer[kColorTextfieldOutline] = {kColorSysNeutralOutline};
  mixer[kColorTextfieldOutlineDisabled] = {SK_ColorTRANSPARENT};
  mixer[kColorTextfieldOutlineInvalid] = {
      kColorTextfieldForegroundPlaceholderInvalid};
  mixer[kColorTextfieldSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorTextSelectionBackground] = {kColorSysTonalContainer};
  mixer[kColorThemeColorPickerCheckmarkBackground] = {kColorSysPrimary};
  mixer[kColorThemeColorPickerCheckmarkForeground] = {kColorSysOnPrimary};
  mixer[kColorThemeColorPickerCustomColorIconBackground] = {
      kColorSysOnSurfaceSubtle};
  mixer[kColorThemeColorPickerHueSliderDialogBackground] = {kColorSysSurface};
  mixer[kColorThemeColorPickerHueSliderDialogForeground] = {kColorSysOnSurface};
  mixer[kColorThemeColorPickerHueSliderDialogIcon] = {kColorSysOnSurfaceSubtle};
  mixer[kColorThemeColorPickerHueSliderHandle] = {kColorSysWhite};
  mixer[kColorThemeColorPickerOptionBackground] = {kColorSysNeutralContainer};
  mixer[kColorThrobber] = {kColorSysPrimary};
  mixer[kColorToastBackground] = {kColorSysInverseSurface};
  mixer[kColorToastBackgroundProminent] = {kColorSysInverseSurfacePrimary};
  mixer[kColorToastButton] = {kColorSysInversePrimary};
  mixer[kColorToastForeground] = {kColorSysInverseOnSurface};
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
