// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/ui_color_mixer.h"

#include <utility>

#include "build/build_config.h"
#include "ui/color/color_id.h"
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
  mixer[kColorBadgeBackground] =
      PickGoogleColor(kColorAccent, kColorDialogBackground,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorBadgeForeground] = GetColorWithMaxContrast(kColorBadgeBackground);
  mixer[kColorBadgeInCocoaMenuBackground] = {kColorBadgeBackground};
  mixer[kColorBadgeInCocoaMenuForeground] = {kColorBadgeForeground};
  mixer[kColorBubbleBackground] = {kColorPrimaryBackground};
  mixer[kColorBubbleBorder] = {kColorMidground};
  mixer[kColorBubbleBorderShadowLarge] = {SetAlpha(kColorShadowBase, 0x1A)};
  mixer[kColorBubbleBorderShadowSmall] = {SetAlpha(kColorShadowBase, 0x33)};
  mixer[kColorBubbleBorderWhenShadowPresent] = {SetAlpha(SK_ColorBLACK, 0x26)};
  mixer[kColorBubbleFooterBackground] = {kColorSubtleEmphasisBackground};
  mixer[kColorBubbleFooterBorder] = {kColorMidground};
  mixer[kColorButtonBackground] = {kColorPrimaryBackground};
  mixer[kColorButtonBackgroundPressed] = {kColorButtonBackground};
  mixer[kColorButtonBackgroundProminent] =
      PickGoogleColor(kColorAccent, kColorDialogBackground,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorButtonBackgroundProminentDisabled] = {
      kColorSubtleEmphasisBackground};
  mixer[kColorButtonBackgroundProminentFocused] = {
      kColorButtonBackgroundProminent};
  mixer[kColorButtonBackgroundTonal] = {kColorSysPrimaryContainer};
  mixer[kColorButtonBackgroundTonalDisabled] = {kColorSubtleEmphasisBackground};
  mixer[kColorButtonBackgroundTonalFocused] = {kColorButtonBackgroundTonal};
  mixer[kColorButtonBorder] = {kColorMidground};
  mixer[kColorButtonBorderDisabled] = {kColorSubtleEmphasisBackground};
  mixer[kColorButtonForeground] =
      PickGoogleColor(kColorAccent, kColorButtonBackground,
                      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorButtonForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorButtonForegroundProminent] =
      GetColorWithMaxContrast(kColorButtonBackgroundProminent);
  mixer[kColorButtonForegroundTonal] = {kColorSysOnPrimaryContainer};
  mixer[kColorButtonHoverBackgroundText] = {kColorSysStateHoverOnSubtle};
  mixer[kColorCheckboxForegroundUnchecked] = {kColorSecondaryForeground};
  mixer[kColorCheckboxForegroundChecked] = {kColorButtonForeground};
  mixer[kColorCustomFrameCaptionForeground] = {SK_ColorWHITE};
  mixer[kColorDebugBoundsOutline] = SetAlpha(SK_ColorRED, 0x30);
  mixer[kColorDebugContentOutline] = SetAlpha(SK_ColorBLUE, 0x30);
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
  mixer[kColorInfoBarIcon] = {kColorAccent};
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
  mixer[kColorLiveCaptionBubbleBackgroundDefault] = {
      SkColorSetA(gfx::kGoogleGrey900, 0xE6)};
  mixer[kColorLiveCaptionBubbleButtonIcon] =
      ui::DeriveDefaultIconColor(kColorLiveCaptionBubbleForegroundDefault);
  mixer[kColorLiveCaptionBubbleButtonIconDisabled] = ui::SetAlpha(
      kColorLiveCaptionBubbleButtonIcon, gfx::kDisabledControlAlpha);
  mixer[kColorLiveCaptionBubbleForegroundDefault] =
      GetColorWithMaxContrast(kColorLiveCaptionBubbleBackgroundDefault);
  mixer[kColorLiveCaptionBubbleForegroundSecondary] = PickGoogleColor(
      GetResultingPaintColor(
          SetAlpha(kColorLiveCaptionBubbleForegroundDefault, 0x8C),
          kColorLiveCaptionBubbleBackgroundDefault),
      kColorLiveCaptionBubbleBackgroundDefault,
      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorLiveCaptionBubbleCheckbox] = PickGoogleColor(
      kColorAccent, kColorLiveCaptionBubbleBackgroundDefault, 6.0f);
  mixer[kColorLiveCaptionBubbleLink] = {kColorLiveCaptionBubbleCheckbox};
  mixer[kColorMenuBackground] = {kColorPrimaryBackground};
  mixer[kColorMenuBorder] = {kColorMidground};
  mixer[kColorMenuButtonBackground] = {kColorMenuBackground};
  mixer[kColorMenuDropmarker] = {kColorPrimaryForeground};
  mixer[kColorMenuIcon] = {kColorIcon};
  mixer[kColorMenuItemBackgroundAlertedInitial] =
      SetAlpha(kColorAccentWithGuaranteedContrastAtopPrimaryBackground, 0x4D);
  mixer[kColorMenuItemBackgroundAlertedTarget] =
      SetAlpha(kColorAccentWithGuaranteedContrastAtopPrimaryBackground,
               gfx::kGoogleGreyAlpha200);
  mixer[kColorMenuItemBackgroundHighlighted] = {kColorSubtleEmphasisBackground};
  mixer[kColorMenuItemBackgroundSelected] = {kColorMenuSelectionBackground};
  mixer[kColorMenuItemForeground] = {kColorPrimaryForeground};
  mixer[kColorMenuItemForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorMenuItemForegroundHighlighted] = {kColorMenuItemForeground};
  mixer[kColorMenuItemForegroundSecondary] = {kColorSecondaryForeground};
  mixer[kColorMenuItemForegroundSelected] = {kColorMenuItemForeground};
  mixer[kColorMenuSeparator] = {kColorSeparator};
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
  mixer[kColorNotificationInputBackground] = PickGoogleColorTwoBackgrounds(
      kColorAccent, kColorNotificationBackgroundActive,
      kColorNotificationBackgroundInactive,
      color_utils::kMinimumVisibleContrastRatio);
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
  mixer[kColorProgressBarPaused] = {kColorDisabledForeground};
  mixer[kColorProgressBar] =
      PickGoogleColor(kColorAccent, kColorDialogBackground,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorRadioButtonForegroundChecked] = {kColorButtonForeground};
  mixer[kColorRadioButtonForegroundUnchecked] = {kColorSecondaryForeground};
  mixer[kColorScrollbarArrowBackgroundHovered] = {
      dark_mode ? SkColorSetRGB(0x4F, 0x4F, 0x4F)
                : SkColorSetRGB(0xD2, 0xD2, 0xD2)};
  mixer[kColorScrollbarArrowBackgroundPressed] = {
      dark_mode ? SkColorSetRGB(0xB1, 0xB1, 0xB1)
                : SkColorSetRGB(0x78, 0x78, 0x78)};
  mixer[kColorScrollbarArrowForeground] = {
      dark_mode ? SK_ColorWHITE : SkColorSetRGB(0x50, 0x50, 0x50)};
  mixer[kColorScrollbarArrowForegroundPressed] = {dark_mode ? SK_ColorBLACK
                                                            : SK_ColorWHITE};
  mixer[kColorScrollbarCorner] = {dark_mode ? SkColorSetRGB(0x12, 0x12, 0x12)
                                            : SkColorSetRGB(0xDC, 0xDC, 0xDC)};
  mixer[kColorScrollbarThumb] = {dark_mode ? SkColorSetA(SK_ColorWHITE, 0x33)
                                           : SkColorSetA(SK_ColorBLACK, 0x33)};
  mixer[kColorScrollbarThumbHovered] = {dark_mode
                                            ? SkColorSetA(SK_ColorWHITE, 0x4D)
                                            : SkColorSetA(SK_ColorBLACK, 0x4D)};
  mixer[kColorScrollbarThumbInactive] = {
      dark_mode ? SK_ColorWHITE : SkColorSetRGB(0xEA, 0xEA, 0xEA)};
  mixer[kColorScrollbarThumbPressed] = {dark_mode
                                            ? SkColorSetA(SK_ColorWHITE, 0x80)
                                            : SkColorSetA(SK_ColorBLACK, 0x80)};
  mixer[kColorScrollbarTrack] = {dark_mode ? SkColorSetRGB(0x42, 0x42, 0x42)
                                           : SkColorSetRGB(0xF1, 0xF1, 0xF1)};
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
  mixer[kColorSidePanelComboboxBorder] = {SK_ColorTRANSPARENT};
  mixer[kColorSidePanelComboboxBackground] = {kColorSubtleEmphasisBackground};
  mixer[kColorSliderThumb] =
      PickGoogleColor(kColorAccent, kColorDialogBackground,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorSliderThumbMinimal] = {kColorSecondaryForeground};
  mixer[kColorSliderTrack] = {kColorSubtleAccent};
  mixer[kColorSliderTrackMinimal] = {kColorMidground};
  mixer[kColorSyncInfoBackground] = {kColorSubtleEmphasisBackground};
  mixer[kColorSyncInfoBackgroundError] =
      SetAlpha(kColorAlertHighSeverity, gfx::kGoogleGreyAlpha100);
  mixer[kColorSyncInfoBackgroundPaused] =
      SetAlpha(kColorAccentWithGuaranteedContrastAtopPrimaryBackground,
               gfx::kGoogleGreyAlpha100);
  {
    auto tab_background_base =
        PickGoogleColor(kColorAccent, kColorPrimaryBackground, 6.0f);
    mixer[kColorTabBackgroundHighlighted] = SetAlpha(tab_background_base, 0x2B);
    mixer[kColorTabBackgroundHighlightedFocused] =
        SetAlpha(std::move(tab_background_base), 0x53);
  }
  mixer[kColorTabBorderSelected] =
      PickGoogleColor(kColorAccent, kColorDialogBackground,
                      color_utils::kMinimumVisibleContrastRatio);
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
  mixer[kColorTextfieldForegroundPlaceholderInvalid] = {
      kColorTextfieldForegroundPlaceholder};
  mixer[kColorTextfieldSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorTextfieldSelectionForeground] = {kColorTextSelectionForeground};
  mixer[kColorTextfieldOutline] = {kColorFocusableBorderUnfocused};
  mixer[kColorTextfieldDisabledOutline] = {kColorFocusableBorderUnfocused};
  mixer[kColorTextfieldInvalidOutline] = {kColorAlertHighSeverity};
  mixer[kColorThrobber] =
      PickGoogleColor(kColorAccent, kColorDialogBackground,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorThrobberPreconnect] = {kColorSubtleAccent};
  mixer[kColorToggleButtonShadow] = {kColorMidground};
  mixer[kColorToggleButtonThumbOff] = {kColorSecondaryForeground};
  mixer[kColorToggleButtonThumbOn] =
      PickGoogleColor(kColorAccent, kColorDialogBackground,
                      color_utils::kMinimumVisibleContrastRatio);
  if (dark_mode) {
    mixer[kColorToggleButtonThumbOff] +=
        AlphaBlend(kColorPrimaryForeground, FromTransformInput(), 0x0D);
    mixer[kColorToggleButtonThumbOn] +=
        AlphaBlend(kColorPrimaryForeground, FromTransformInput(), 0x0D);
  }
  mixer[kColorToggleButtonTrackOff] = {
      dark_mode ? ColorTransform(gfx::kGoogleGrey700) : kColorMidground};
  mixer[kColorToggleButtonTrackOn] =
      PickGoogleColor(kColorAccent, kColorToggleButtonThumbOn, 2.13f);
  mixer[kColorTooltipBackground] = GetResultingPaintColor(
      SetAlpha(kColorPrimaryBackground, 0xCC), {kColorWindowBackground});
  mixer[kColorTooltipForeground] = GetResultingPaintColor(
      SetAlpha(kColorPrimaryForeground, 0xDE), {kColorTooltipBackground});
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
