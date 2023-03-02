// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_ID_H_
#define UI_COLOR_COLOR_ID_H_

#include "base/check_op.h"
#include "build/build_config.h"
#include "build/buildflag.h"

// clang-format off
#define CROSS_PLATFORM_COLOR_IDS \
  /* UI reference color tokens */ \
  /* Use the 3 param macro so kColorAccent is set to the correct value. */ \
  E_CPONLY(kColorRefPrimary0, kUiColorsStart, kUiColorsStart) \
  E_CPONLY(kColorRefPrimary10) \
  E_CPONLY(kColorRefPrimary20) \
  E_CPONLY(kColorRefPrimary30) \
  E_CPONLY(kColorRefPrimary40) \
  E_CPONLY(kColorRefPrimary50) \
  E_CPONLY(kColorRefPrimary60) \
  E_CPONLY(kColorRefPrimary70) \
  E_CPONLY(kColorRefPrimary80) \
  E_CPONLY(kColorRefPrimary90) \
  E_CPONLY(kColorRefPrimary95) \
  E_CPONLY(kColorRefPrimary99) \
  E_CPONLY(kColorRefPrimary100) \
  E_CPONLY(kColorRefSecondary0) \
  E_CPONLY(kColorRefSecondary10) \
  E_CPONLY(kColorRefSecondary20) \
  E_CPONLY(kColorRefSecondary30) \
  E_CPONLY(kColorRefSecondary40) \
  E_CPONLY(kColorRefSecondary50) \
  E_CPONLY(kColorRefSecondary60) \
  E_CPONLY(kColorRefSecondary70) \
  E_CPONLY(kColorRefSecondary80) \
  E_CPONLY(kColorRefSecondary90) \
  E_CPONLY(kColorRefSecondary95) \
  E_CPONLY(kColorRefSecondary99) \
  E_CPONLY(kColorRefSecondary100) \
  E_CPONLY(kColorRefTertiary0) \
  E_CPONLY(kColorRefTertiary10) \
  E_CPONLY(kColorRefTertiary20) \
  E_CPONLY(kColorRefTertiary30) \
  E_CPONLY(kColorRefTertiary40) \
  E_CPONLY(kColorRefTertiary50) \
  E_CPONLY(kColorRefTertiary60) \
  E_CPONLY(kColorRefTertiary70) \
  E_CPONLY(kColorRefTertiary80) \
  E_CPONLY(kColorRefTertiary90) \
  E_CPONLY(kColorRefTertiary95) \
  E_CPONLY(kColorRefTertiary99) \
  E_CPONLY(kColorRefTertiary100) \
  E_CPONLY(kColorRefError0) \
  E_CPONLY(kColorRefError10) \
  E_CPONLY(kColorRefError20) \
  E_CPONLY(kColorRefError30) \
  E_CPONLY(kColorRefError40) \
  E_CPONLY(kColorRefError50) \
  E_CPONLY(kColorRefError60) \
  E_CPONLY(kColorRefError70) \
  E_CPONLY(kColorRefError80) \
  E_CPONLY(kColorRefError90) \
  E_CPONLY(kColorRefError95) \
  E_CPONLY(kColorRefError99) \
  E_CPONLY(kColorRefError100) \
  E_CPONLY(kColorRefNeutral0) \
  E_CPONLY(kColorRefNeutral10) \
  E_CPONLY(kColorRefNeutral20) \
  E_CPONLY(kColorRefNeutral30) \
  E_CPONLY(kColorRefNeutral40) \
  E_CPONLY(kColorRefNeutral50) \
  E_CPONLY(kColorRefNeutral60) \
  E_CPONLY(kColorRefNeutral70) \
  E_CPONLY(kColorRefNeutral80) \
  E_CPONLY(kColorRefNeutral90) \
  E_CPONLY(kColorRefNeutral95) \
  E_CPONLY(kColorRefNeutral99) \
  E_CPONLY(kColorRefNeutral100) \
  E_CPONLY(kColorRefNeutralVariant0) \
  E_CPONLY(kColorRefNeutralVariant10) \
  E_CPONLY(kColorRefNeutralVariant20) \
  E_CPONLY(kColorRefNeutralVariant30) \
  E_CPONLY(kColorRefNeutralVariant40) \
  E_CPONLY(kColorRefNeutralVariant50) \
  E_CPONLY(kColorRefNeutralVariant60) \
  E_CPONLY(kColorRefNeutralVariant70) \
  E_CPONLY(kColorRefNeutralVariant80) \
  E_CPONLY(kColorRefNeutralVariant90) \
  E_CPONLY(kColorRefNeutralVariant95) \
  E_CPONLY(kColorRefNeutralVariant99) \
  E_CPONLY(kColorRefNeutralVariant100) \
  \
  /* UI system color tokens */ \
  E_CPONLY(kColorSysPrimary) \
  E_CPONLY(kColorSysOnPrimary) \
  E_CPONLY(kColorSysPrimaryContainer) \
  E_CPONLY(kColorSysOnPrimaryContainer) \
  E_CPONLY(kColorSysSecondary) \
  E_CPONLY(kColorSysOnSecondary) \
  E_CPONLY(kColorSysSecondaryContainer) \
  E_CPONLY(kColorSysOnSecondaryContainer) \
  E_CPONLY(kColorSysTertiary) \
  E_CPONLY(kColorSysOnTertiary) \
  E_CPONLY(kColorSysTertiaryContainer) \
  E_CPONLY(kColorSysOnTertiaryContainer) \
  E_CPONLY(kColorSysError) \
  E_CPONLY(kColorSysOnError) \
  E_CPONLY(kColorSysErrorContainer) \
  E_CPONLY(kColorSysOnErrorContainer) \
  E_CPONLY(kColorSysSurfaceVariant) \
  E_CPONLY(kColorSysOnSurfaceVariant) \
  E_CPONLY(kColorSysOutline) \
  E_CPONLY(kColorSysScrim) \
  E_CPONLY(kColorSysSeparator) \
  E_CPONLY(kColorSysSurface) \
  E_CPONLY(kColorSysSurface1) \
  E_CPONLY(kColorSysSurface2) \
  E_CPONLY(kColorSysSurface3) \
  E_CPONLY(kColorSysSurface4) \
  E_CPONLY(kColorSysSurface5) \
  /* Core color concepts */ \
  /* kColorAccent is used in color_provider_css_colors_test.ts. */ \
  /* If changing the variable name, the variable name in the test needs to */ \
  /* be changed as well. */ \
  E_CPONLY(kColorAccent) \
  E_CPONLY(kColorAccentWithGuaranteedContrastAtopPrimaryBackground) \
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
  /* This is declared here so src/components/ can access it, but we expect */ \
  /* this to be set in the embedder. */ \
  E_CPONLY(kColorInfoBarIcon) \
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
  E_CPONLY(kColorProgressBarPaused) \
  E_CPONLY(kColorReadAnythingBackground) \
  E_CPONLY(kColorReadAnythingBackgroundBlue) \
  E_CPONLY(kColorReadAnythingBackgroundDark) \
  E_CPONLY(kColorReadAnythingBackgroundLight) \
  E_CPONLY(kColorReadAnythingBackgroundYellow) \
  E_CPONLY(kColorReadAnythingForeground) \
  E_CPONLY(kColorReadAnythingForegroundBlue) \
  E_CPONLY(kColorReadAnythingForegroundDark) \
  E_CPONLY(kColorReadAnythingForegroundLight) \
  E_CPONLY(kColorReadAnythingForegroundYellow) \
  E_CPONLY(kColorScrollbarArrowBackgroundHovered) \
  E_CPONLY(kColorScrollbarArrowBackgroundPressed) \
  E_CPONLY(kColorScrollbarArrowForeground) \
  E_CPONLY(kColorScrollbarArrowForegroundPressed) \
  E_CPONLY(kColorScrollbarCorner) \
  E_CPONLY(kColorScrollbarThumb) \
  E_CPONLY(kColorScrollbarThumbHovered) \
  E_CPONLY(kColorScrollbarThumbInactive) \
  E_CPONLY(kColorScrollbarThumbPressed) \
  E_CPONLY(kColorScrollbarTrack) \
  E_CPONLY(kColorSeparator) \
  E_CPONLY(kColorShadowBase) \
  E_CPONLY(kColorShadowValueAmbientShadowElevationSixteen) \
  E_CPONLY(kColorShadowValueAmbientShadowElevationThree) \
  E_CPONLY(kColorShadowValueKeyShadowElevationSixteen) \
  E_CPONLY(kColorShadowValueKeyShadowElevationThree) \
  E_CPONLY(kColorSidePanelComboboxBorder) \
  E_CPONLY(kColorSidePanelComboboxBackground) \
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
  /* NOTE: Nearly all of the following CrOS color ids will need to be re- */ \
  /* evaluated once CrOS fully supports the color pipeline. */ \
  E_CPONLY(kColorAshActionLabelFocusRingEdit) \
  E_CPONLY(kColorAshActionLabelFocusRingError) \
  E_CPONLY(kColorAshActionLabelFocusRingHover) \
  \
  /* TODO(skau): Remove Compat value when dark/light mode launches. */ \
  E_CPONLY(kColorAshAppListFocusRingCompat) \
  E_CPONLY(kColorAshAppListFocusRingNoKeyboard) \
  E_CPONLY(kColorAshAppListSeparator) \
  E_CPONLY(kColorAshAppListSeparatorLight) \
  E_CPONLY(kColorAshArcInputMenuSeparator) \
  E_CPONLY(kColorAshFocusRing) \
  /* TODO(kylixrd): Determine whether this special color should follow */ \
  /* light/dark mode. Remove if it should equal kColorAshFocusRing. */ \
  E_CPONLY(kColorAshEditFinishFocusRing) \
  E_CPONLY(kColorAshIconInOobe) \
  \
  /* TODO(crbug/1319917): Remove these when dark light mode is launched. */ \
  E_CPONLY(kColorAshLightFocusRing) \
  \
  E_CPONLY(kColorAshOnboardingFocusRing) \
  \
  E_CPONLY(kColorAshPrivacyIndicatorsBackground) \
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
  \
  E_CPONLY(kColorHighlightBorderBorder1) \
  E_CPONLY(kColorHighlightBorderBorder2) \
  E_CPONLY(kColorHighlightBorderBorder3) \
  E_CPONLY(kColorHighlightBorderHighlight1) \
  E_CPONLY(kColorHighlightBorderHighlight2) \
  E_CPONLY(kColorHighlightBorderHighlight3) \
  \
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

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/color/color_id_macros.inc"  // NOLINT(build/include)

}  // namespace ui

#endif  // UI_COLOR_COLOR_ID_H_
