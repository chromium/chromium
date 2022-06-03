// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_ID_H_
#define UI_COLOR_COLOR_ID_H_

#include "base/check_op.h"
#include "build/build_config.h"
#include "build/buildflag.h"

// clang-format off
#define CROSS_PLATFORM_COLOR_IDS \
  /* Core color concepts */ \
  /* Use the 3 param macro so kColorAccent is set to the correct value. */ \
  /* The kColorAccent color id is used in color_provider_css_colors_test.ts. \
  If changing the variable name, the variable name in the test needs to be \
  changed as well. */ \
  E_CPONLY(kColorAccent, kUiColorsStart, kUiColorsStart) \
  E_CPONLY(kColorAlertHighSeverity) \
  E_CPONLY(kColorAlertLowSeverity) \
  E_CPONLY(kColorAlertMediumSeverity) \
  E_CPONLY(kColorDisabledForeground) \
  E_CPONLY(kColorEndpointBackground) \
  E_CPONLY(kColorEndpointForeground) \
  E_CPONLY(kColorItemHighlight) \
  E_CPONLY(kColorItemSelectionBackground) \
  E_CPONLY(kColorMenuSelectionBackground) \
  E_CPONLY(kColorMidground) \
  E_CPONLY(kColorPrimaryBackground) \
  E_CPONLY(kColorPrimaryForeground) \
  E_CPONLY(kColorSecondaryForeground) \
  E_CPONLY(kColorSubtleAccent) \
  E_CPONLY(kColorSubtleEmphasisBackground) \
  E_CPONLY(kColorTextSelectionBackground) \
  E_CPONLY(kColorTextSelectionForeground) \
  \
  /* Further UI element colors */ \
  E_CPONLY(kColorAvatarHeaderArt) \
  E_CPONLY(kColorAvatarIconGuest) \
  E_CPONLY(kColorAvatarIconIncognito) \
  E_CPONLY(kColorBubbleBackground) \
  E_CPONLY(kColorBubbleBorder) \
  E_CPONLY(kColorBubbleBorderShadowLarge) \
  E_CPONLY(kColorBubbleBorderShadowSmall) \
  E_CPONLY(kColorBubbleBorderWhenShadowPresent) \
  E_CPONLY(kColorBubbleFooterBackground) \
  E_CPONLY(kColorBubbleFooterBorder) \
  E_CPONLY(kColorButtonBackground) \
  E_CPONLY(kColorButtonBackgroundPressed) \
  E_CPONLY(kColorButtonBackgroundProminent) \
  E_CPONLY(kColorButtonBackgroundProminentDisabled) \
  E_CPONLY(kColorButtonBackgroundProminentFocused) \
  E_CPONLY(kColorButtonBorder) \
  E_CPONLY(kColorButtonBorderDisabled) \
  E_CPONLY(kColorButtonForeground) \
  E_CPONLY(kColorButtonForegroundChecked) \
  E_CPONLY(kColorButtonForegroundDisabled) \
  E_CPONLY(kColorButtonForegroundProminent) \
  E_CPONLY(kColorButtonForegroundUnchecked) \
  E_CPONLY(kColorCustomFrameCaptionForeground) \
  E_CPONLY(kColorDebugBoundsOutline) \
  E_CPONLY(kColorDebugContentOutline) \
  E_CPONLY(kColorDialogBackground) \
  E_CPONLY(kColorDialogForeground) \
  E_CPONLY(kColorDropdownBackground) \
  E_CPONLY(kColorDropdownBackgroundSelected) \
  E_CPONLY(kColorDropdownForeground) \
  E_CPONLY(kColorDropdownForegroundSelected) \
  E_CPONLY(kColorFocusableBorderFocused) \
  E_CPONLY(kColorFocusableBorderUnfocused) \
  E_CPONLY(kColorFrameActive) \
  E_CPONLY(kColorFrameActiveUnthemed) \
  E_CPONLY(kColorFrameInactive) \
  E_CPONLY(kColorHelpIconActive) \
  E_CPONLY(kColorHelpIconInactive) \
  E_CPONLY(kColorIcon) \
  E_CPONLY(kColorIconDisabled) \
  E_CPONLY(kColorIconSecondary) \
  E_CPONLY(kColorLabelForeground) \
  E_CPONLY(kColorLabelForegroundDisabled) \
  E_CPONLY(kColorLabelForegroundSecondary) \
  E_CPONLY(kColorLabelSelectionBackground) \
  E_CPONLY(kColorLabelSelectionForeground) \
  E_CPONLY(kColorLinkForeground) \
  E_CPONLY(kColorLinkForegroundDisabled) \
  E_CPONLY(kColorLinkForegroundPressed) \
  E_CPONLY(kColorLiveCaptionBubbleBackgroundDefault) \
  E_CPONLY(kColorLiveCaptionBubbleButtonIcon) \
  E_CPONLY(kColorLiveCaptionBubbleButtonIconDisabled) \
  E_CPONLY(kColorLiveCaptionBubbleForegroundDefault) \
  E_CPONLY(kColorLiveCaptionBubbleCheckbox) \
  E_CPONLY(kColorLiveCaptionBubbleLink) \
  E_CPONLY(kColorMenuBackground) \
  E_CPONLY(kColorMenuBorder) \
  E_CPONLY(kColorMenuDropmarker) \
  E_CPONLY(kColorMenuIcon) \
  E_CPONLY(kColorMenuItemBackgroundAlertedInitial) \
  E_CPONLY(kColorMenuItemBackgroundAlertedTarget) \
  E_CPONLY(kColorMenuItemBackgroundHighlighted) \
  E_CPONLY(kColorMenuItemBackgroundSelected) \
  E_CPONLY(kColorMenuItemForeground) \
  E_CPONLY(kColorMenuItemForegroundDisabled) \
  E_CPONLY(kColorMenuItemForegroundHighlighted) \
  E_CPONLY(kColorMenuItemForegroundSecondary) \
  E_CPONLY(kColorMenuItemForegroundSelected) \
  E_CPONLY(kColorMenuSeparator) \
  E_CPONLY(kColorNotificationActionsBackground) \
  E_CPONLY(kColorNotificationBackgroundActive) \
  E_CPONLY(kColorNotificationBackgroundInactive) \
  E_CPONLY(kColorNotificationHeaderForeground) \
  E_CPONLY(kColorNotificationIconBackground) \
  E_CPONLY(kColorNotificationIconForeground) \
  E_CPONLY(kColorNotificationImageBackground) \
  E_CPONLY(kColorNotificationInputBackground) \
  E_CPONLY(kColorNotificationInputForeground) \
  E_CPONLY(kColorNotificationInputPlaceholderForeground) \
  E_CPONLY(kColorOverlayScrollbarFill) \
  E_CPONLY(kColorOverlayScrollbarFillDark) \
  E_CPONLY(kColorOverlayScrollbarFillLight) \
  E_CPONLY(kColorOverlayScrollbarFillHovered) \
  E_CPONLY(kColorOverlayScrollbarFillHoveredDark) \
  E_CPONLY(kColorOverlayScrollbarFillHoveredLight) \
  E_CPONLY(kColorOverlayScrollbarStroke) \
  E_CPONLY(kColorOverlayScrollbarStrokeDark) \
  E_CPONLY(kColorOverlayScrollbarStrokeLight) \
  E_CPONLY(kColorOverlayScrollbarStrokeHovered) \
  E_CPONLY(kColorOverlayScrollbarStrokeHoveredDark) \
  E_CPONLY(kColorOverlayScrollbarStrokeHoveredLight) \
  E_CPONLY(kColorProgressBar) \
  E_CPONLY(kColorSeparator) \
  E_CPONLY(kColorShadowBase) \
  E_CPONLY(kColorShadowValueAmbientShadowElevationSixteen) \
  E_CPONLY(kColorShadowValueAmbientShadowElevationThree) \
  E_CPONLY(kColorShadowValueKeyShadowElevationSixteen) \
  E_CPONLY(kColorShadowValueKeyShadowElevationThree) \
  E_CPONLY(kColorSliderThumb) \
  E_CPONLY(kColorSliderThumbMinimal) \
  E_CPONLY(kColorSliderTrack) \
  E_CPONLY(kColorSliderTrackMinimal) \
  E_CPONLY(kColorSyncInfoBackground) \
  E_CPONLY(kColorSyncInfoBackgroundError) \
  E_CPONLY(kColorSyncInfoBackgroundPaused) \
  E_CPONLY(kColorTabBackgroundHighlighted) \
  E_CPONLY(kColorTabBackgroundHighlightedFocused) \
  E_CPONLY(kColorTabBorderSelected) \
  E_CPONLY(kColorTabContentSeparator) \
  E_CPONLY(kColorTabForeground) \
  E_CPONLY(kColorTabForegroundSelected) \
  E_CPONLY(kColorTableBackground) \
  E_CPONLY(kColorTableBackgroundAlternate) \
  E_CPONLY(kColorTableBackgroundSelectedFocused) \
  E_CPONLY(kColorTableBackgroundSelectedUnfocused) \
  E_CPONLY(kColorTableForeground) \
  E_CPONLY(kColorTableForegroundSelectedFocused) \
  E_CPONLY(kColorTableForegroundSelectedUnfocused) \
  E_CPONLY(kColorTableGroupingIndicator) \
  E_CPONLY(kColorTableHeaderBackground) \
  E_CPONLY(kColorTableHeaderForeground) \
  E_CPONLY(kColorTableHeaderSeparator) \
  E_CPONLY(kColorTextfieldBackground) \
  E_CPONLY(kColorTextfieldBackgroundDisabled) \
  E_CPONLY(kColorTextfieldForeground) \
  E_CPONLY(kColorTextfieldForegroundDisabled) \
  E_CPONLY(kColorTextfieldForegroundPlaceholder) \
  E_CPONLY(kColorTextfieldSelectionBackground) \
  E_CPONLY(kColorTextfieldSelectionForeground) \
  E_CPONLY(kColorThrobber) \
  E_CPONLY(kColorThrobberPreconnect) \
  E_CPONLY(kColorToggleButtonShadow) \
  E_CPONLY(kColorToggleButtonThumbOff) \
  E_CPONLY(kColorToggleButtonThumbOn) \
  E_CPONLY(kColorToggleButtonTrackOff) \
  E_CPONLY(kColorToggleButtonTrackOn) \
  E_CPONLY(kColorTooltipBackground) \
  E_CPONLY(kColorTooltipForeground) \
  E_CPONLY(kColorTreeBackground) \
  E_CPONLY(kColorTreeNodeBackgroundSelectedFocused) \
  E_CPONLY(kColorTreeNodeBackgroundSelectedUnfocused) \
  E_CPONLY(kColorTreeNodeForeground) \
  E_CPONLY(kColorTreeNodeForegroundSelectedFocused) \
  E_CPONLY(kColorTreeNodeForegroundSelectedUnfocused) \
  E_CPONLY(kColorWindowBackground)

#if BUILDFLAG(IS_CHROMEOS)
#define PLATFORM_SPECIFIC_COLOR_IDS \
  E_CPONLY(kColorAshSystemUIBorderColor1) \
  E_CPONLY(kColorAshSystemUIBorderColor2) \
  E_CPONLY(kColorAshSystemUIHighlightColor1) \
  E_CPONLY(kColorAshSystemUIHighlightColor2) \
  \
  /* TODO(crbug/1319917): Remove these when dark light mode is launched. */ \
  E_CPONLY(kColorAshSystemUILightBorderColor1) \
  E_CPONLY(kColorAshSystemUILightBorderColor2) \
  E_CPONLY(kColorAshSystemUILightHighlightColor1) \
  E_CPONLY(kColorAshSystemUILightHighlightColor2) \
  \
  E_CPONLY(kColorAshSystemUIMenuBackground) \
  E_CPONLY(kColorAshSystemUIMenuIcon) \
  E_CPONLY(kColorAshSystemUIMenuItemBackgroundSelected) \
  E_CPONLY(kColorAshSystemUIMenuSeparator) \
  E_CPONLY(kColorNativeColor1) \
  E_CPONLY(kColorNativeColor1Shade1) \
  E_CPONLY(kColorNativeColor1Shade2) \
  E_CPONLY(kColorNativeColor2) \
  E_CPONLY(kColorNativeColor3) \
  E_CPONLY(kColorNativeColor4) \
  E_CPONLY(kColorNativeColor5) \
  E_CPONLY(kColorNativeColor6) \
  E_CPONLY(kColorNativeBaseColor) \
  E_CPONLY(kColorNativeSecondaryColor)
#elif BUILDFLAG(IS_LINUX)
#define PLATFORM_SPECIFIC_COLOR_IDS \
  E_CPONLY(kColorNativeButtonBorder)\
  E_CPONLY(kColorNativeHeaderButtonBorderActive) \
  E_CPONLY(kColorNativeHeaderButtonBorderInactive) \
  E_CPONLY(kColorNativeHeaderSeparatorBorderActive) \
  E_CPONLY(kColorNativeHeaderSeparatorBorderInactive) \
  E_CPONLY(kColorNativeLabelForeground) \
  E_CPONLY(kColorNativeTabForegroundInactiveFrameActive) \
  E_CPONLY(kColorNativeTabForegroundInactiveFrameInactive) \
  E_CPONLY(kColorNativeTextfieldBorderUnfocused)\
  E_CPONLY(kColorNativeToolbarBackground)
#elif BUILDFLAG(IS_WIN)
#define PLATFORM_SPECIFIC_COLOR_IDS \
  E_CPONLY(kColorNative3dDkShadow) \
  E_CPONLY(kColorNative3dLight) \
  E_CPONLY(kColorNativeActiveBorder) \
  E_CPONLY(kColorNativeActiveCaption) \
  E_CPONLY(kColorNativeAppWorkspace) \
  E_CPONLY(kColorNativeBackground) \
  E_CPONLY(kColorNativeBtnFace) \
  E_CPONLY(kColorNativeBtnHighlight) \
  E_CPONLY(kColorNativeBtnShadow) \
  E_CPONLY(kColorNativeBtnText) \
  E_CPONLY(kColorNativeCaptionText) \
  E_CPONLY(kColorNativeGradientActiveCaption) \
  E_CPONLY(kColorNativeGradientInactiveCaption) \
  E_CPONLY(kColorNativeGrayText) \
  E_CPONLY(kColorNativeHighlight) \
  E_CPONLY(kColorNativeHighlightText) \
  E_CPONLY(kColorNativeHotlight) \
  E_CPONLY(kColorNativeInactiveBorder) \
  E_CPONLY(kColorNativeInactiveCaption) \
  E_CPONLY(kColorNativeInactiveCaptionText) \
  E_CPONLY(kColorNativeInfoBk) \
  E_CPONLY(kColorNativeInfoText) \
  E_CPONLY(kColorNativeMenu) \
  E_CPONLY(kColorNativeMenuBar) \
  E_CPONLY(kColorNativeMenuHilight) \
  E_CPONLY(kColorNativeMenuText) \
  E_CPONLY(kColorNativeScrollbar) \
  E_CPONLY(kColorNativeWindow) \
  E_CPONLY(kColorNativeWindowFrame) \
  E_CPONLY(kColorNativeWindowText)
#else
#define PLATFORM_SPECIFIC_COLOR_IDS
#endif

#define COLOR_IDS \
  CROSS_PLATFORM_COLOR_IDS \
  PLATFORM_SPECIFIC_COLOR_IDS
// clang-format on

namespace ui {

#include "ui/color/color_id_macros.inc"

// ColorId contains identifiers for all input, intermediary, and output colors
// known to the core UI layer.  Embedders can extend this enum with additional
// values that are understood by the ColorProvider implementation.  Embedders
// define enum values from kUiColorsEnd.  Values named beginning with "kColor"
// represent the actual colors; the rest are markers.
using ColorId = int;
// clang-format off
enum ColorIds : ColorId {
  kUiColorsStart = 0,

  COLOR_IDS

  // TODO(pkasting): Other native colors

  // Embedders must start color IDs from this value.
  kUiColorsEnd,

  // Embedders must not assign IDs larger than this value.  This is used to
  // verify that color IDs and color set IDs are not interchanged.
  kUiColorsLast = 0xffff
};
// clang-format on

#include "ui/color/color_id_macros.inc"

}  // namespace ui

#endif  // UI_COLOR_COLOR_ID_H_
