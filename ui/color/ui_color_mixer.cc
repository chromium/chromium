// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/ui_color_mixer.h"

#include <utility>

#include "build/build_config.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"

namespace ui {

void AddUiColorMixer(ColorProvider* provider, const ColorProviderKey& key) {
  const bool dark_mode = key.color_mode == ColorProviderKey::ColorMode::kDark;

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
  mixer[kColorBubbleBorder] = {kColorSysSurfaceVariant};
  mixer[kColorBubbleBorderShadowLarge] = {SetAlpha(kColorShadowBase, 0x1A)};
  mixer[kColorBubbleBorderShadowSmall] = {SetAlpha(kColorShadowBase, 0x33)};
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
  mixer[kColorButtonBackgroundTonal] = {kColorSysTonalContainer};
  mixer[kColorButtonBackgroundTonalDisabled] = {
      kColorButtonBackgroundProminentDisabled};
  mixer[kColorButtonBackgroundTonalFocused] = {kColorButtonBackgroundTonal};
  mixer[kColorButtonBackgroundWithAttention] = {
      dark_mode ? SkColorSetRGB(0x35, 0x36, 0x3A) : SK_ColorWHITE};
  mixer[kColorButtonBorder] = {kColorMidground};
  mixer[kColorButtonBorderDisabled] = {kColorSubtleEmphasisBackground};
  mixer[kColorButtonFeatureAttentionHighlight] =
      ui::PickGoogleColor(ui::kColorAccent, kColorButtonBackgroundWithAttention,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorButtonForeground] =
      PickGoogleColor(kColorAccent, kColorButtonBackground,
                      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorButtonForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorButtonForegroundProminent] =
      GetColorWithMaxContrast(kColorButtonBackgroundProminent);
  mixer[kColorButtonForegroundTonal] = {kColorSysOnTonalContainer};
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
      PickGoogleColor(kColorLinkForegroundDefault, kColorDialogBackground,
                      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorLinkForegroundDefault] = {kColorAccent};
  mixer[kColorLinkForegroundOnBubbleFooter] =
      PickGoogleColor(kColorLinkForegroundDefault, kColorBubbleFooterBackground,
                      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorLinkForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorLinkForegroundPressed] = PickGoogleColor(
      kColorLinkForegroundPressedDefault, kColorDialogBackground,
      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorLinkForegroundPressedDefault] = {kColorLinkForegroundDefault};
  mixer[kColorLinkForegroundPressedOnBubbleFooter] = PickGoogleColor(
      kColorLinkForegroundPressedDefault, kColorBubbleFooterBackground,
      color_utils::kMinimumReadableContrastRatio);
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
  mixer[kColorMenuButtonBackgroundSelected] = {
      kColorMenuItemBackgroundSelected};
  mixer[kColorMenuDropmarker] = {kColorPrimaryForeground};
  mixer[kColorMenuIcon] = {kColorIcon};
  mixer[kColorMenuIconDisabled] = {kColorIconDisabled};
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
  mixer[kColorOverlayScrollbarFillHovered] =
      SetAlpha(kColorEndpointForeground, gfx::kGoogleGreyAlpha800);
  mixer[kColorOverlayScrollbarStroke] =
      SetAlpha(kColorEndpointBackground, gfx::kGoogleGreyAlpha400);
  mixer[kColorOverlayScrollbarStrokeHovered] =
      SetAlpha(kColorEndpointBackground, gfx::kGoogleGreyAlpha500);
  mixer[kColorProgressBar] = PickGoogleColorTwoBackgrounds(
      kColorAccent, kColorDialogBackground, kColorProgressBarBackground,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorProgressBarBackground] = {kColorMidground};
  mixer[kColorProgressBarPaused] = {kColorDisabledForeground};
  mixer[kColorRadioButtonForegroundChecked] = {kColorButtonForeground};
  mixer[kColorRadioButtonForegroundUnchecked] = {kColorSecondaryForeground};
  mixer[kColorSeparator] = {kColorMidground};
  mixer[kColorShadowBase] = {dark_mode ? SK_ColorBLACK : gfx::kGoogleGrey800};
  mixer[kColorShadowValueAmbientShadowElevationThree] =
      SetAlpha(kColorShadowBase, 0x40);
  mixer[kColorShadowValueKeyShadowElevationThree] =
      SetAlpha(kColorShadowBase, 0x66);
  mixer[kColorShadowValueAmbientShadowElevationFour] =
      SetAlpha(SK_ColorBLACK, 0x3d);
  mixer[kColorShadowValueKeyShadowElevationFour] =
      SetAlpha(SK_ColorBLACK, 0x1f);
  mixer[kColorShadowValueAmbientShadowElevationTwelve] = {
      kColorShadowValueAmbientShadowElevationFour};
  mixer[kColorShadowValueKeyShadowElevationTwelve] = {
      kColorShadowValueKeyShadowElevationFour};
  mixer[kColorShadowValueAmbientShadowElevationSixteen] =
      SetAlpha(kColorShadowBase, 0x3d);
  mixer[kColorShadowValueKeyShadowElevationSixteen] =
      SetAlpha(kColorShadowBase, 0x1a);
  mixer[kColorShadowValueAmbientShadowElevationTwentyFour] = {
      kColorShadowValueAmbientShadowElevationFour};
  mixer[kColorShadowValueKeyShadowElevationTwentyFour] = {
      kColorShadowValueKeyShadowElevationFour};

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
  mixer[kColorTabContentSeparator] = {kColorMidground};
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
  mixer[kColorTextfieldHover] = {SK_ColorTRANSPARENT};
  mixer[kColorTextfieldOutline] = {kColorFocusableBorderUnfocused};
  mixer[kColorTextfieldOutlineDisabled] = {kColorFocusableBorderUnfocused};
  mixer[kColorTextfieldOutlineInvalid] = {kColorAlertHighSeverity};
  mixer[kColorTextfieldSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorTextfieldSelectionForeground] = {kColorTextSelectionForeground};
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
  mixer[kColorWebNativeControlAccent] = {dark_mode
                                             ? SkColorSetRGB(0x99, 0xC8, 0xFF)
                                             : SkColorSetRGB(0x00, 0x75, 0xFF)};
  mixer[kColorWebNativeControlAccentDisabled] = {
      dark_mode ? SkColorSetRGB(0x75, 0x75, 0x75)
                : SkColorSetARGB(0x4D, 0x76, 0x76, 0x76)};
  mixer[kColorWebNativeControlAccentHovered] = {
      dark_mode ? SkColorSetRGB(0xD1, 0xE6, 0xFF)
                : SkColorSetRGB(0x00, 0x5C, 0xC8)};
  mixer[kColorWebNativeControlAccentPressed] = {
      dark_mode ? SkColorSetRGB(0x61, 0xA9, 0xFF)
                : SkColorSetRGB(0x37, 0x93, 0xFF)};
  mixer[kColorWebNativeControlAutoCompleteBackground] = {
      dark_mode ? SkColorSetARGB(0x66, 0x46, 0x5a, 0x7E)
                : SkColorSetRGB(0xE8, 0xF0, 0xFE)};
  mixer[kColorWebNativeControlBackground] = {
      dark_mode ? SkColorSetRGB(0x3B, 0x3B, 0x3B) : SK_ColorWHITE};
  mixer[kColorWebNativeControlBackgroundDisabled] = {
      dark_mode ? SkColorSetRGB(0x3B, 0x3B, 0x3B)
                : SkColorSetA(SK_ColorWHITE, 0x99)};
  mixer[kColorWebNativeControlBorder] = {dark_mode
                                             ? SkColorSetRGB(0x85, 0x85, 0x85)
                                             : SkColorSetRGB(0x76, 0x76, 0x76)};
  mixer[kColorWebNativeControlBorderDisabled] = {
      dark_mode ? SkColorSetRGB(0x62, 0x62, 0x62)
                : SkColorSetARGB(0x4D, 0x76, 0x76, 0x76)};
  mixer[kColorWebNativeControlBorderHovered] = {
      dark_mode ? SkColorSetRGB(0xAC, 0xAC, 0xAC)
                : SkColorSetRGB(0x4F, 0x4F, 0x4F)};
  mixer[kColorWebNativeControlBorderPressed] = {
      dark_mode ? SkColorSetRGB(0x6E, 0x6E, 0x6E)
                : SkColorSetRGB(0x8D, 0x8D, 0x8D)};
  mixer[kColorWebNativeControlButtonBorder] = {
      dark_mode ? SkColorSetRGB(0x6B, 0x6B, 0x6B)
                : SkColorSetRGB(0x76, 0x76, 0x76)};
  mixer[kColorWebNativeControlButtonBorderDisabled] = {
      dark_mode ? SkColorSetRGB(0x36, 0x36, 0x36)
                : SkColorSetARGB(0x4D, 0x76, 0x76, 0x76)};
  mixer[kColorWebNativeControlButtonBorderHovered] = {
      dark_mode ? SkColorSetRGB(0x7B, 0x7B, 0x7B)
                : SkColorSetRGB(0x4F, 0x4F, 0x4F)};
  mixer[kColorWebNativeControlButtonBorderPressed] = {
      dark_mode ? SkColorSetRGB(0x61, 0x61, 0x61)
                : SkColorSetRGB(0x8D, 0x8D, 0x8D)};
  mixer[kColorWebNativeControlButtonFill] = {
      dark_mode ? SkColorSetRGB(0x6B, 0x6B, 0x6B)
                : SkColorSetRGB(0xEF, 0xEF, 0xEF)};
  mixer[kColorWebNativeControlButtonFillDisabled] = {
      dark_mode ? SkColorSetRGB(0x36, 0x36, 0x36)
                : SkColorSetARGB(0x4D, 0xEF, 0xEF, 0xEF)};
  mixer[kColorWebNativeControlButtonFillHovered] = {
      dark_mode ? SkColorSetRGB(0x7B, 0x7B, 0x7B)
                : SkColorSetRGB(0xE5, 0xE5, 0xE5)};
  mixer[kColorWebNativeControlButtonFillPressed] = {
      dark_mode ? SkColorSetRGB(0x61, 0x61, 0x61)
                : SkColorSetRGB(0xF5, 0xF5, 0xF5)};
  mixer[kColorWebNativeControlFill] = {dark_mode
                                           ? SkColorSetRGB(0x3B, 0x3B, 0x3B)
                                           : SkColorSetRGB(0xEF, 0xEF, 0xEF)};
  mixer[kColorWebNativeControlFillDisabled] = {
      dark_mode ? SkColorSetRGB(0x36, 0x36, 0x36)
                : SkColorSetARGB(0x4D, 0xEF, 0xEF, 0xEF)};
  mixer[kColorWebNativeControlFillHovered] = {
      dark_mode ? SkColorSetRGB(0x3B, 0x3B, 0x3B)
                : SkColorSetRGB(0xE5, 0xE5, 0xE5)};
  mixer[kColorWebNativeControlFillPressed] = {
      dark_mode ? SkColorSetRGB(0x3B, 0x3B, 0x3B)
                : SkColorSetRGB(0xF5, 0xF5, 0xF5)};
  mixer[kColorWebNativeControlLightenLayer] = {
      dark_mode ? SkColorSetRGB(0x3B, 0x3B, 0x3B)
                : SkColorSetARGB(0x33, 0xA9, 0xA9, 0xA9)};
  mixer[kColorWebNativeControlProgressValue] = {
      dark_mode ? SkColorSetRGB(0x63, 0xAD, 0xE5)
                : SkColorSetRGB(0x00, 0x75, 0xFF)};
  mixer[kColorWebNativeControlScrollbarArrowBackgroundHovered] = {
      dark_mode ? SkColorSetRGB(0x4F, 0x4F, 0x4F)
                : SkColorSetRGB(0xD2, 0xD2, 0xD2)};
  mixer[kColorWebNativeControlScrollbarArrowBackgroundPressed] = {
      dark_mode ? SkColorSetRGB(0xB1, 0xB1, 0xB1)
                : SkColorSetRGB(0x78, 0x78, 0x78)};
  mixer[kColorWebNativeControlScrollbarArrowForeground] = {
      dark_mode ? SK_ColorWHITE : SkColorSetRGB(0x50, 0x50, 0x50)};
  mixer[kColorWebNativeControlScrollbarArrowForegroundPressed] = {
      dark_mode ? SK_ColorBLACK : SK_ColorWHITE};
  mixer[kColorWebNativeControlScrollbarCorner] = {
      dark_mode ? SkColorSetRGB(0x12, 0x12, 0x12)
                : SkColorSetRGB(0xDC, 0xDC, 0xDC)};
  mixer[kColorWebNativeControlScrollbarThumb] = {
      dark_mode ? SkColorSetA(SK_ColorWHITE, 0x33)
                : SkColorSetA(SK_ColorBLACK, 0x33)};
  mixer[kColorWebNativeControlScrollbarThumbHovered] = {
      dark_mode ? SkColorSetA(SK_ColorWHITE, 0x4D)
                : SkColorSetA(SK_ColorBLACK, 0x4D)};
  mixer[kColorWebNativeControlScrollbarThumbInactive] = {
      dark_mode ? SK_ColorWHITE : SkColorSetRGB(0xEA, 0xEA, 0xEA)};
  mixer[kColorWebNativeControlScrollbarThumbOverlayMinimalMode] = {
      dark_mode ? SkColorSetA(SK_ColorWHITE, 0x8B)
                : SkColorSetA(SK_ColorBLACK, 0x72)};
  mixer[kColorWebNativeControlScrollbarThumbPressed] = {
      dark_mode ? SkColorSetA(SK_ColorWHITE, 0x80)
                : SkColorSetA(SK_ColorBLACK, 0x80)};
  mixer[kColorWebNativeControlScrollbarTrack] = {
      dark_mode ? SkColorSetRGB(0x42, 0x42, 0x42)
                : SkColorSetRGB(0xF1, 0xF1, 0xF1)};
  mixer[kColorWebNativeControlSlider] = {dark_mode
                                             ? SkColorSetRGB(0x99, 0xC8, 0xFF)
                                             : SkColorSetRGB(0x00, 0x75, 0xFF)};
  mixer[kColorWebNativeControlSliderDisabled] = {
      dark_mode ? SkColorSetRGB(0x75, 0x75, 0x75)
                : SkColorSetRGB(0xCB, 0xCB, 0xCB)};
  mixer[kColorWebNativeControlSliderHovered] = {
      dark_mode ? SkColorSetRGB(0xD1, 0xE6, 0xFF)
                : SkColorSetRGB(0x00, 0x5C, 0xC8)};
  mixer[kColorWebNativeControlSliderPressed] = {
      dark_mode ? SkColorSetRGB(0x61, 0xA9, 0xFF)
                : SkColorSetRGB(0x37, 0x93, 0xFF)};
  mixer[kColorWindowBackground] = {kColorPrimaryBackground};
}

}  // namespace ui
