// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/ui_color_mixer.h"

#include "build/build_config.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"

namespace ui {

void AddUiColorMixer(ColorProvider* provider,
                     const ColorProviderManager::Key& key) {
  const bool dark_mode =
      key.color_mode == ColorProviderManager::ColorMode::kDark;

  ColorMixer& mixer = provider->AddMixer();
  mixer[kColorAvatarHeaderArt] = {kColorMidground};
  mixer[kColorAvatarIconGuest] = {kColorSecondaryForeground};
  mixer[kColorAvatarIconIncognito] = {kColorPrimaryForeground};
  mixer[kColorBubbleBackground] = {kColorPrimaryBackground};
  mixer[kColorBubbleBorder] = {kColorMidground};
  mixer[kColorBubbleBorderShadowLarge] = {SetAlpha(kColorShadowBase, 0x1A)};
  mixer[kColorBubbleBorderShadowSmall] = {SetAlpha(kColorShadowBase, 0x33)};
  mixer[kColorBubbleBorderWhenShadowPresent] = {SetAlpha(SK_ColorBLACK, 0x26)};
  mixer[kColorBubbleFooterBackground] = {kColorSubtleEmphasisBackground};
  mixer[kColorBubbleFooterBorder] = {kColorMidground};
  mixer[kColorButtonBackground] = {kColorPrimaryBackground};
  mixer[kColorButtonBackgroundPressed] = {kColorButtonBackground};
  mixer[kColorButtonBackgroundProminent] = {kColorAccent};
  mixer[kColorButtonBackgroundProminentDisabled] = {
      kColorSubtleEmphasisBackground};
  mixer[kColorButtonBackgroundProminentFocused] = {
      kColorButtonBackgroundProminent};
  mixer[kColorButtonBorder] = {kColorMidground};
  mixer[kColorButtonBorderDisabled] = {kColorSubtleEmphasisBackground};
  mixer[kColorButtonForeground] =
      PickGoogleColor(kColorAccent, kColorButtonBackground,
                      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorButtonForegroundChecked] = {kColorButtonForeground};
  mixer[kColorButtonForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorButtonForegroundProminent] =
      GetColorWithMaxContrast(kColorButtonBackgroundProminent);
  mixer[kColorButtonForegroundUnchecked] = {kColorSecondaryForeground};
  mixer[kColorDialogBackground] = {kColorPrimaryBackground};
  mixer[kColorDialogForeground] = {kColorSecondaryForeground};
  mixer[kColorDropdownBackground] = {kColorPrimaryBackground};
  mixer[kColorDropdownBackgroundSelected] = {kColorMenuSelectionBackground};
  mixer[kColorDropdownForeground] = {kColorPrimaryForeground};
  mixer[kColorDropdownForegroundSelected] = {kColorPrimaryForeground};
  mixer[kColorFocusableBorderFocused] = {kColorItemHighlight};
  mixer[kColorFocusableBorderUnfocused] = {kColorMidground};
  mixer[kColorFrameActive] = {kColorFrameActiveUnthemed};
  mixer[kColorFrameActiveUnthemed] = {
      dark_mode ? gfx::kGoogleGrey900 : SkColorSetRGB(0xDE, 0xE1, 0xE6)};
  mixer[kColorFrameInactive] = {dark_mode ? gfx::kGoogleGrey800
                                          : gfx::kGoogleGrey200};
  mixer[kColorHelpIconActive] = {kColorPrimaryForeground};
  mixer[kColorHelpIconInactive] = {kColorSecondaryForeground};
  mixer[kColorIcon] = {kColorSecondaryForeground};
  mixer[kColorIconDisabled] = SetAlpha(kColorIcon, gfx::kDisabledControlAlpha);
  mixer[kColorIconSecondary] = {gfx::kGoogleGrey600};
  mixer[kColorLabelForeground] = {kColorPrimaryForeground};
  mixer[kColorLabelForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorLabelForegroundSecondary] = {kColorSecondaryForeground};
  mixer[kColorLabelSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorLabelSelectionForeground] = {kColorTextSelectionForeground};
  mixer[kColorLinkForeground] =
      PickGoogleColor(kColorAccent, kColorDialogBackground,
                      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorLinkForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorLinkForegroundPressed] = {kColorLinkForeground};
  mixer[kColorMenuBackground] = {kColorPrimaryBackground};
  mixer[kColorMenuBorder] = {kColorMidground};
  mixer[kColorMenuDropmarker] = {kColorPrimaryForeground};
  mixer[kColorMenuIcon] = {kColorIcon};
  mixer[kColorMenuItemBackgroundAlertedInitial] = SetAlpha(kColorAccent, 0x4D);
  mixer[kColorMenuItemBackgroundAlertedTarget] =
      SetAlpha(kColorAccent, gfx::kGoogleGreyAlpha200);
  mixer[kColorMenuItemBackgroundHighlighted] = {kColorSubtleEmphasisBackground};
  mixer[kColorMenuItemBackgroundSelected] = {kColorMenuSelectionBackground};
  mixer[kColorMenuItemForeground] = {kColorPrimaryForeground};
  mixer[kColorMenuItemForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorMenuItemForegroundHighlighted] = {kColorMenuItemForeground};
  mixer[kColorMenuItemForegroundSecondary] = {kColorSecondaryForeground};
  mixer[kColorMenuItemForegroundSelected] = {kColorMenuItemForeground};
  mixer[kColorMenuSeparator] = {kColorMidground};
  mixer[kColorNotificationActionsBackground] = {
      kColorNotificationBackgroundActive};
  mixer[kColorNotificationBackgroundActive] = {kColorSubtleEmphasisBackground};
  mixer[kColorNotificationBackgroundInactive] = {kColorPrimaryBackground};
  mixer[kColorNotificationHeaderForeground] = {kColorSecondaryForeground};
  mixer[kColorNotificationIconBackground] = {
      kColorNotificationHeaderForeground};
  mixer[kColorNotificationIconForeground] = {
      kColorNotificationBackgroundInactive};
  mixer[kColorNotificationImageBackground] = {
      kColorNotificationBackgroundActive};
  mixer[kColorNotificationInputBackground] = {kColorAccent};
  mixer[kColorNotificationInputForeground] =
      GetColorWithMaxContrast(kColorNotificationInputBackground);
  mixer[kColorNotificationInputPlaceholderForeground] =
      SetAlpha(kColorNotificationInputForeground, gfx::kGoogleGreyAlpha700);
  mixer[kColorOverlayScrollbarFill] =
      SetAlpha(kColorEndpointForeground, gfx::kGoogleGreyAlpha700);
  mixer[kColorOverlayScrollbarFillDark] = SetAlpha(
      GetColorWithMaxContrast(SK_ColorWHITE), gfx::kGoogleGreyAlpha700);
  mixer[kColorOverlayScrollbarFillLight] = SetAlpha(
      GetColorWithMaxContrast(SK_ColorBLACK), gfx::kGoogleGreyAlpha700);
  mixer[kColorOverlayScrollbarFillHovered] =
      SetAlpha(kColorEndpointForeground, gfx::kGoogleGreyAlpha800);
  mixer[kColorOverlayScrollbarFillHoveredDark] = SetAlpha(
      GetColorWithMaxContrast(SK_ColorWHITE), gfx::kGoogleGreyAlpha800);
  mixer[kColorOverlayScrollbarFillHoveredLight] = SetAlpha(
      GetColorWithMaxContrast(SK_ColorBLACK), gfx::kGoogleGreyAlpha800);
  mixer[kColorOverlayScrollbarStroke] =
      SetAlpha(kColorEndpointBackground, gfx::kGoogleGreyAlpha400);
  mixer[kColorOverlayScrollbarStrokeDark] =
      SetAlpha(GetColorWithMaxContrast(kColorOverlayScrollbarFillDark),
               gfx::kGoogleGreyAlpha400);
  mixer[kColorOverlayScrollbarStrokeLight] =
      SetAlpha(GetColorWithMaxContrast(kColorOverlayScrollbarFillLight),
               gfx::kGoogleGreyAlpha400);
  mixer[kColorOverlayScrollbarStrokeHovered] =
      SetAlpha(kColorEndpointBackground, gfx::kGoogleGreyAlpha500);
  mixer[kColorOverlayScrollbarStrokeHoveredDark] =
      SetAlpha(GetColorWithMaxContrast(kColorOverlayScrollbarFillHoveredDark),
               gfx::kGoogleGreyAlpha500);
  mixer[kColorOverlayScrollbarStrokeHoveredLight] =
      SetAlpha(GetColorWithMaxContrast(kColorOverlayScrollbarFillHoveredLight),
               gfx::kGoogleGreyAlpha500);
  mixer[kColorProgressBar] = {kColorAccent};
  mixer[kColorPwaSecurityChipForeground] = {kColorSecondaryForeground};
  mixer[kColorPwaSecurityChipForegroundDangerous] = {kColorAlertHighSeverity};
  mixer[kColorPwaSecurityChipForegroundPolicyCert] = {kColorDisabledForeground};
  mixer[kColorPwaSecurityChipForegroundSecure] = {
      kColorPwaSecurityChipForeground};
  mixer[kColorPwaToolbarBackground] = {kColorEndpointBackground};
  mixer[kColorPwaToolbarForeground] = {kColorEndpointForeground};
  mixer[kColorSeparator] = {kColorMidground};
  mixer[kColorShadowBase] = {dark_mode ? SK_ColorBLACK : gfx::kGoogleGrey800};
  mixer[kColorShadowValueAmbientShadowElevationThree] =
      SetAlpha(kColorShadowBase, 0x40);
  mixer[kColorShadowValueKeyShadowElevationThree] =
      SetAlpha(kColorShadowBase, 0x66);
  mixer[kColorShadowValueAmbientShadowElevationSixteen] =
      SetAlpha(kColorShadowBase, 0x3d);
  mixer[kColorShadowValueKeyShadowElevationSixteen] =
      SetAlpha(kColorShadowBase, 0x1a);
  mixer[kColorSliderThumb] = {kColorAccent};
  mixer[kColorSliderThumbMinimal] = {kColorSecondaryForeground};
  mixer[kColorSliderTrack] = {kColorSubtleAccent};
  mixer[kColorSliderTrackMinimal] = {kColorMidground};
  mixer[kColorSyncInfoBackground] = {kColorSubtleEmphasisBackground};
  mixer[kColorSyncInfoBackgroundError] =
      SetAlpha(kColorAlertHighSeverity, gfx::kGoogleGreyAlpha100);
  mixer[kColorSyncInfoBackgroundPaused] =
      SetAlpha(kColorAccent, gfx::kGoogleGreyAlpha100);
  mixer[kColorTabBackgroundHighlighted] = SetAlpha(gfx::kGoogleBlue300, 0x2B);
  mixer[kColorTabBackgroundHighlightedFocused] =
      SetAlpha(gfx::kGoogleBlue300, 0x53);
  mixer[kColorTabBorderSelected] = {kColorAccent};
  mixer[kColorTabContentSeparator] = {kColorMidground};
  mixer[kColorTabForeground] = {kColorSecondaryForeground};
  mixer[kColorTabForegroundSelected] =
      PickGoogleColor(kColorAccent, kColorDialogBackground,
                      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorTableBackground] = {kColorPrimaryBackground};
  mixer[kColorTableBackgroundAlternate] = {kColorTableBackground};
  mixer[kColorTableBackgroundSelectedFocused] = {kColorItemSelectionBackground};
  mixer[kColorTableBackgroundSelectedUnfocused] = {
      kColorTableBackgroundSelectedFocused};
  mixer[kColorTableForeground] = {kColorPrimaryForeground};
  mixer[kColorTableForegroundSelectedFocused] = {kColorTableForeground};
  mixer[kColorTableForegroundSelectedUnfocused] = {
      kColorTableForegroundSelectedFocused};
  mixer[kColorTableGroupingIndicator] = {kColorItemHighlight};
  mixer[kColorTableHeaderBackground] = {kColorTableBackground};
  mixer[kColorTableHeaderForeground] = {kColorTableForeground};
  mixer[kColorTableHeaderSeparator] = {kColorMidground};
  mixer[kColorTextfieldBackground] = {kColorEndpointBackground};
  mixer[kColorTextfieldBackgroundDisabled] = {kColorPrimaryBackground};
  mixer[kColorTextfieldForeground] = {kColorPrimaryForeground};
  mixer[kColorTextfieldForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorTextfieldForegroundPlaceholder] = {
      kColorTextfieldForegroundDisabled};
  mixer[kColorTextfieldSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorTextfieldSelectionForeground] = {kColorTextSelectionForeground};
  mixer[kColorThrobber] = {kColorAccent};
  mixer[kColorThrobberPreconnect] = {kColorSubtleAccent};
  mixer[kColorToggleButtonShadow] = {kColorMidground};
  mixer[kColorToggleButtonThumbOff] = {kColorSecondaryForeground};
  mixer[kColorToggleButtonThumbOn] = {kColorAccent};
  if (dark_mode) {
    mixer[kColorToggleButtonThumbOff] +=
        AlphaBlend(kColorPrimaryForeground, FromTransformInput(), 0x0D);
    mixer[kColorToggleButtonThumbOn] +=
        AlphaBlend(kColorPrimaryForeground, FromTransformInput(), 0x0D);
  }
  mixer[kColorToggleButtonTrackOff] = {
      dark_mode ? ColorTransform(gfx::kGoogleGrey700) : kColorMidground};
  mixer[kColorToggleButtonTrackOn] = {dark_mode ? gfx::kGoogleBlue600
                                                : gfx::kGoogleBlue300};
  mixer[kColorTooltipBackground] = SetAlpha(kColorPrimaryBackground, 0xCC);
  mixer[kColorTooltipForeground] = SetAlpha(kColorPrimaryForeground, 0xDE);
  mixer[kColorTreeBackground] = {kColorPrimaryBackground};
  mixer[kColorTreeNodeBackgroundSelectedFocused] = {
      kColorItemSelectionBackground};
  mixer[kColorTreeNodeBackgroundSelectedUnfocused] = {
      kColorTreeNodeBackgroundSelectedFocused};
  mixer[kColorTreeNodeForeground] = {kColorPrimaryForeground};
  mixer[kColorTreeNodeForegroundSelectedFocused] = {kColorTreeNodeForeground};
  mixer[kColorTreeNodeForegroundSelectedUnfocused] = {
      kColorTreeNodeForegroundSelectedFocused};
  mixer[kColorWindowBackground] = {kColorPrimaryBackground};
}

}  // namespace ui
