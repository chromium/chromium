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
  E(kColorButtonBackgroundProminent, \
    NativeTheme::kColorId_ProminentButtonColor) \
  E_CPONLY(kColorButtonBackgroundProminentDisabled) \
  E_CPONLY(kColorButtonBackgroundProminentFocused) \
  E_CPONLY(kColorButtonBorder) \
  E_CPONLY(kColorButtonBorderDisabled) \
  E_CPONLY(kColorButtonForeground) \
  E_CPONLY(kColorButtonForegroundChecked) \
  E_CPONLY(kColorButtonForegroundDisabled) \
  E(kColorButtonForegroundProminent, \
    NativeTheme::kColorId_TextOnProminentButtonColor) \
  E_CPONLY(kColorButtonForegroundUnchecked) \
  E_CPONLY(kColorDialogBackground) \
  E_CPONLY(kColorDialogForeground) \
  E_CPONLY(kColorDropdownBackground) \
  E_CPONLY(kColorDropdownBackgroundSelected) \
  E_CPONLY(kColorDropdownForeground) \
  E_CPONLY(kColorDropdownForegroundSelected) \
  E(kColorFocusableBorderFocused, NativeTheme::kColorId_FocusedBorderColor) \
  E_CPONLY(kColorFocusableBorderUnfocused) \
  E_CPONLY(kColorFrameActive) \
  E_CPONLY(kColorFrameInactive) \
  E_CPONLY(kColorHelpIconActive) \
  E_CPONLY(kColorHelpIconInactive) \
  E(kColorIcon, NativeTheme::kColorId_DefaultIconColor) \
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
  E(kColorMenuBackground, NativeTheme::kColorId_MenuBackgroundColor) \
  E_CPONLY(kColorMenuBorder) \
  E_CPONLY(kColorMenuDropmarker) \
  E(kColorMenuIcon, NativeTheme::kColorId_MenuIconColor) \
  E_CPONLY(kColorMenuItemBackgroundAlertedInitial) \
  E_CPONLY(kColorMenuItemBackgroundAlertedTarget) \
  E_CPONLY(kColorMenuItemBackgroundHighlighted) \
  E(kColorMenuItemBackgroundSelected, \
    NativeTheme::kColorId_FocusedMenuItemBackgroundColor) \
  E_CPONLY(kColorMenuItemForeground) \
  E_CPONLY(kColorMenuItemForegroundDisabled) \
  E_CPONLY(kColorMenuItemForegroundHighlighted) \
  E_CPONLY(kColorMenuItemForegroundSecondary) \
  E_CPONLY(kColorMenuItemForegroundSelected) \
  E(kColorMenuSeparator, NativeTheme::kColorId_MenuSeparatorColor) \
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
  E(kColorOverlayScrollbarFill, \
    NativeTheme::kColorId_OverlayScrollbarThumbFill) \
  E(kColorOverlayScrollbarFillHovered, \
    NativeTheme::kColorId_OverlayScrollbarThumbHoveredFill) \
  E(kColorOverlayScrollbarStroke, \
    NativeTheme::kColorId_OverlayScrollbarThumbStroke) \
  E(kColorOverlayScrollbarStrokeHovered, \
    NativeTheme::kColorId_OverlayScrollbarThumbHoveredStroke) \
  E_CPONLY(kColorProgressBar) \
  E_CPONLY(kColorPwaSecurityChipForeground) \
  E_CPONLY(kColorPwaSecurityChipForegroundDangerous) \
  E_CPONLY(kColorPwaSecurityChipForegroundPolicyCert) \
  E_CPONLY(kColorPwaSecurityChipForegroundSecure) \
  E_CPONLY(kColorPwaToolbarBackground) \
  E_CPONLY(kColorPwaToolbarForeground) \
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
  E(kColorThrobber, NativeTheme::kColorId_ThrobberSpinningColor) \
  E(kColorThrobberPreconnect, NativeTheme::kColorId_ThrobberWaitingColor) \
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
  E(kColorWindowBackground, NativeTheme::kColorId_WindowBackground)

#if defined(OS_WIN)
#define WIN_COLOR_IDS \
  /* Windows native colors */ \
  E(kColorNative3dDkShadow, COLOR_3DDKSHADOW) \
  E(kColorNative3dLight, COLOR_3DLIGHT) \
  E(kColorNativeActiveBorder, COLOR_ACTIVEBORDER) \
  E(kColorNativeActiveCaption, COLOR_ACTIVECAPTION) \
  E(kColorNativeAppWorkspace, COLOR_APPWORKSPACE) \
  E(kColorNativeBackground, COLOR_BACKGROUND) \
  E(kColorNativeBtnFace, COLOR_BTNFACE) \
  E(kColorNativeBtnHighlight, COLOR_BTNHIGHLIGHT) \
  E(kColorNativeBtnShadow, COLOR_BTNSHADOW) \
  E(kColorNativeBtnText, COLOR_BTNTEXT) \
  E(kColorNativeCaptionText, COLOR_CAPTIONTEXT) \
  E(kColorNativeGradientActiveCaption, COLOR_GRADIENTACTIVECAPTION) \
  E(kColorNativeGradientInactiveCaption, COLOR_GRADIENTINACTIVECAPTION) \
  E(kColorNativeGrayText, COLOR_GRAYTEXT) \
  E(kColorNativeHighlight, COLOR_HIGHLIGHT) \
  E(kColorNativeHighlightText, COLOR_HIGHLIGHTTEXT) \
  E(kColorNativeHotlight, COLOR_HOTLIGHT) \
  E(kColorNativeInactiveBorder, COLOR_INACTIVEBORDER) \
  E(kColorNativeInactiveCaption, COLOR_INACTIVECAPTION) \
  E(kColorNativeInactiveCaptionText, COLOR_INACTIVECAPTIONTEXT) \
  E(kColorNativeInfoBk, COLOR_INFOBK) \
  E(kColorNativeInfoText, COLOR_INFOTEXT) \
  E(kColorNativeMenu, COLOR_MENU) \
  E(kColorNativeMenuBar, COLOR_MENUBAR) \
  E(kColorNativeMenuHilight, COLOR_MENUHILIGHT) \
  E(kColorNativeMenuText, COLOR_MENUTEXT) \
  E(kColorNativeScrollbar, COLOR_SCROLLBAR) \
  E(kColorNativeWindow, COLOR_WINDOW) \
  E(kColorNativeWindowFrame, COLOR_WINDOWFRAME) \
  E(kColorNativeWindowText, COLOR_WINDOWTEXT)
#endif

#if defined(OS_WIN)
#define COLOR_IDS \
  CROSS_PLATFORM_COLOR_IDS \
  WIN_COLOR_IDS
#else
#define COLOR_IDS CROSS_PLATFORM_COLOR_IDS
#endif
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

// ColorSetId contains identifiers for all distinct color sets known to the core
// UI layer.  As with ColorId, embedders can extend this enum with additional
// values that are understood by the ColorProvider implementation.  Embedders
// define enum values from kUiColorSetsEnd.  Values named beginning with
// "kColorSet" represent the actual colors; the rest are markers.
using ColorSetId = int;
enum ColorSetIds : ColorSetId {
  kUiColorSetsStart = kUiColorsLast + 1,

  // A set of color IDs whose values match the native platform as closely as
  // possible.
  kColorSetNative = kUiColorSetsStart,

  // A set of color IDs representing the default values for core color concepts,
  // in the absence of native colors.
  kColorSetCoreDefaults,

  // Embedders must start color set IDs from this value.
  kUiColorSetsEnd,
};

// Verifies that |id| is a color ID, not a color set ID.
#define DCHECK_COLOR_ID_VALID(id) \
  DCHECK_GE(id, kUiColorsStart);  \
  DCHECK_LE(id, kUiColorsLast)

// Verifies that |id| is a color set ID, not a color ID.
#define DCHECK_COLOR_SET_ID_VALID(id) DCHECK_GE(id, kUiColorSetsStart)

}  // namespace ui

#endif  // UI_COLOR_COLOR_ID_H_
