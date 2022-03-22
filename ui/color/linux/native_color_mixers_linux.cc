// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/native_color_mixers.h"

#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace ui {

void AddNativeCoreColorMixer(ColorProvider* provider,
                             const ColorProviderManager::Key& key) {
  if (key.system_theme == ColorProviderManager::SystemTheme::kDefault)
    return;

  ColorMixer& mixer = provider->AddMixer();
  mixer[kColorAccent] = {kColorNativeTreeNodeBackgroundSelectedFocused};
  mixer[kColorAlertHighSeverity] = {SelectBasedOnDarkInput(
      kColorPrimaryBackground, gfx::kGoogleRed300, gfx::kGoogleRed600)};
  mixer[kColorAlertLowSeverity] = {SelectBasedOnDarkInput(
      kColorPrimaryBackground, gfx::kGoogleGreen300, gfx::kGoogleGreen700)};
  mixer[kColorAlertMediumSeverity] = {SelectBasedOnDarkInput(
      kColorPrimaryBackground, gfx::kGoogleYellow300, gfx::kGoogleYellow700)};
  mixer[kColorDisabledForeground] = {kColorNativeLabelForegroundDisabled};
  mixer[kColorItemHighlight] = {kColorNativeTextfieldBorderFocused};
  mixer[kColorItemSelectionBackground] = {kColorAccent};
  mixer[kColorMenuSelectionBackground] = {
      kColorNativeMenuItemBackgroundHovered};
  mixer[kColorMidground] = {kColorNativeSeparator};
  mixer[kColorPrimaryBackground] = {kColorNativeWindowBackground};
  mixer[kColorPrimaryForeground] = {kColorNativeLabelForeground};
  mixer[kColorSecondaryForeground] = {kColorNativeLabelForegroundDisabled};
  mixer[kColorTextSelectionBackground] = {kColorNativeLabelBackgroundSelected};
  mixer[kColorTextSelectionForeground] = {kColorNativeLabelForegroundSelected};
}

void AddNativeUiColorMixer(ColorProvider* provider,
                           const ColorProviderManager::Key& key) {
  if (key.system_theme == ColorProviderManager::SystemTheme::kDefault)
    return;

  ColorMixer& mixer = provider->AddMixer();
  mixer[kColorAvatarHeaderArt] =
      AlphaBlend(kColorPrimaryForeground, kColorPrimaryBackground,
                 gfx::kGoogleGreyAlpha300);
  mixer[kColorAvatarIconGuest] =
      DeriveDefaultIconColor(kColorPrimaryForeground);
  mixer[kColorButtonBackground] = {kColorNativeButtonBackground};
  mixer[kColorButtonBackgroundProminentDisabled] = {
      kColorNativeButtonBackgroundDisabled};
  mixer[kColorButtonBorder] = {kColorNativeButtonBorder};
  mixer[kColorButtonBorderDisabled] = {kColorNativeButtonBackgroundDisabled};
  mixer[kColorButtonForeground] = {kColorNativeButtonForeground};
  mixer[kColorButtonForegroundChecked] = {kColorAccent};
  mixer[kColorButtonForegroundDisabled] = {
      kColorNativeButtonForegroundDisabled};
  mixer[kColorButtonForegroundProminent] = {
      kColorNativeTreeNodeForegroundSelectedFocused};
  mixer[kColorButtonForegroundUnchecked] = {kColorButtonForeground};
  mixer[kColorDialogForeground] = {kColorPrimaryForeground};
  mixer[kColorDropdownBackground] = {kColorNativeComboboxBackground};
  mixer[kColorDropdownBackgroundSelected] = {
      kColorNativeComboboxBackgroundHovered};
  mixer[kColorDropdownForeground] = {kColorNativeComboboxForeground};
  mixer[kColorDropdownForegroundSelected] = {
      kColorNativeComboboxForegroundHovered};
  mixer[kColorFrameActive] = {kColorNativeFrameActive};
  mixer[kColorFrameInactive] = {kColorNativeFrameInactive};
  mixer[kColorFocusableBorderUnfocused] = {
      kColorNativeTextfieldBorderUnfocused};
  mixer[kColorHelpIconActive] = {kColorNativeImageButtonForegroundHovered};
  mixer[kColorIcon] = {kColorNativeButtonIcon};
  mixer[kColorHelpIconInactive] = {kColorNativeImageButtonForeground};
  mixer[kColorLinkForeground] = {kColorNativeLinkForeground};
  mixer[kColorLinkForegroundDisabled] = {kColorNativeLinkForegroundDisabled};
  mixer[kColorLinkForegroundPressed] = {kColorNativeLinkForegroundHovered};
  mixer[kColorMenuBackground] = {kColorNativeMenuBackground};
  mixer[kColorMenuBorder] = {kColorNativeMenuBorder};
  mixer[kColorMenuDropmarker] = {kColorMenuItemForeground};
  mixer[kColorMenuIcon] = {kColorNativeMenuRadio};
  mixer[kColorMenuItemBackgroundHighlighted] = {kColorMenuBackground};
  mixer[kColorMenuItemForeground] = {kColorNativeMenuItemForeground};
  mixer[kColorMenuItemForegroundHighlighted] = {kColorMenuItemForeground};
  mixer[kColorMenuItemForegroundDisabled] = {
      kColorNativeMenuItemForegroundDisabled};
  mixer[kColorMenuItemForegroundSecondary] = {kColorNativeMenuItemAccelerator};
  mixer[kColorMenuItemForegroundSelected] = {
      kColorNativeMenuItemForegroundHovered};
  mixer[kColorMenuSeparator] = {kColorNativeMenuSeparator};
  mixer[kColorNotificationInputForeground] = {
      kColorNativeTreeNodeForegroundSelectedFocused};
  mixer[kColorOverlayScrollbarFill] = {kColorNativeScrollbarSliderBackground};
  mixer[kColorOverlayScrollbarFillHovered] = {
      kColorNativeScrollbarSliderBackgroundHovered};
  mixer[kColorOverlayScrollbarStroke] = {kColorNativeScrollbarTroughBackground};
  mixer[kColorOverlayScrollbarStrokeHovered] = {
      kColorNativeScrollbarTroughBackgroundHovered};
  mixer[kColorSliderThumb] = {kColorNativeScaleHighlightBackground};
  mixer[kColorSliderThumbMinimal] = {
      kColorNativeScaleHighlightBackgroundDisabled};
  mixer[kColorSliderTrack] = {kColorNativeScaleTroughBackground};
  mixer[kColorSliderTrackMinimal] = {kColorNativeScaleTroughBackgroundDisabled};
  mixer[kColorSyncInfoBackground] = {kColorNativeStatusbarBackground};
  mixer[kColorTabBackgroundHighlighted] = {kColorNativeTabBackgroundChecked};
  mixer[kColorTabBackgroundHighlightedFocused] = {
      kColorNativeTabBackgroundCheckedFocused};
  mixer[kColorTabContentSeparator] = {kColorNativeFrameBorder};
  mixer[kColorTabForegroundSelected] = {kColorPrimaryForeground};
  mixer[kColorTableBackground] = {kColorTreeBackground};
  mixer[kColorTableBackgroundAlternate] = {kColorTreeBackground};
  mixer[kColorTableBackgroundSelectedUnfocused] = {
      kColorTreeNodeBackgroundSelectedUnfocused};
  mixer[kColorTableForeground] = {kColorTreeNodeForeground};
  mixer[kColorTableForegroundSelectedFocused] = {
      kColorTreeNodeForegroundSelectedFocused};
  mixer[kColorTableForegroundSelectedUnfocused] = {
      kColorTreeNodeForegroundSelectedUnfocused};
  mixer[kColorTableGroupingIndicator] = {kColorTableForeground};
  mixer[kColorTableHeaderBackground] = {kColorNativeTreeHeaderBackground};
  mixer[kColorTableHeaderForeground] = {kColorNativeTreeHeaderForeground};
  mixer[kColorTableHeaderSeparator] = {kColorNativeTreeHeaderBorder};
  mixer[kColorTextfieldBackground] = {kColorNativeTextareaBackground};
  mixer[kColorTextfieldBackgroundDisabled] = {
      kColorNativeTextareaBackgroundDisabled};
  mixer[kColorTextfieldForeground] = {kColorNativeTextareaForeground};
  mixer[kColorTextfieldForegroundDisabled] = {
      kColorNativeTextareaForegroundDisabled};
  mixer[kColorTextfieldForegroundPlaceholder] = {
      kColorNativeTextfieldForegroundPlaceholder};
  mixer[kColorTextfieldSelectionBackground] = {
      kColorNativeTextareaBackgroundSelected};
  mixer[kColorTextfieldSelectionForeground] = {
      kColorNativeTextareaForegroundSelected};
  mixer[kColorThrobber] = {kColorNativeSpinner};
  mixer[kColorThrobberPreconnect] = {kColorNativeSpinnerDisabled};
  mixer[kColorToggleButtonTrackOff] = {
      kColorNativeToggleButtonBackgroundUnchecked};
  mixer[kColorToggleButtonTrackOn] = {
      kColorNativeToggleButtonBackgroundChecked};
  mixer[kColorTooltipBackground] = {kColorNativeTooltipBackground};
  mixer[kColorTooltipForeground] = {kColorNativeTooltipForeground};
  mixer[kColorTreeBackground] = {kColorNativeTreeNodeBackground};
  mixer[kColorTreeNodeForeground] = {kColorNativeTreeNodeForeground};
  mixer[kColorTreeNodeForegroundSelectedFocused] = {
      kColorNativeTreeNodeForegroundSelectedFocused};
  mixer[kColorTreeNodeBackgroundSelectedUnfocused] = {
      kColorNativeTreeNodeBackgroundSelected};
  mixer[kColorTreeNodeForegroundSelectedUnfocused] = {
      kColorNativeTreeNodeForegroundSelected};
}

}  // namespace ui
