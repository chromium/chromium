// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_ID_H_
#define UI_COLOR_COLOR_ID_H_

#include "base/check_op.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"

// clang-format off
#define CROSS_PLATFORM_COLOR_IDS \
  /* UI reference color tokens */ \
  /* Use the 3 param macro so kColorAccent is set to the correct value. */ \
  E_CPONLY(kColorRefPrimary0, kUiColorsStart, kUiColorsStart) \
  E_CPONLY(kColorRefPrimary10) \
  E_CPONLY(kColorRefPrimary20) \
  E_CPONLY(kColorRefPrimary25) \
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
  E_CPONLY(kColorRefSecondary12) \
  E_CPONLY(kColorRefSecondary15) \
  E_CPONLY(kColorRefSecondary20) \
  E_CPONLY(kColorRefSecondary25) \
  E_CPONLY(kColorRefSecondary30) \
  E_CPONLY(kColorRefSecondary35) \
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
  E_CPONLY(kColorRefNeutral4) \
  E_CPONLY(kColorRefNeutral6) \
  E_CPONLY(kColorRefNeutral8) \
  E_CPONLY(kColorRefNeutral10) \
  E_CPONLY(kColorRefNeutral12) \
  E_CPONLY(kColorRefNeutral15) \
  E_CPONLY(kColorRefNeutral17) \
  E_CPONLY(kColorRefNeutral20) \
  E_CPONLY(kColorRefNeutral22) \
  E_CPONLY(kColorRefNeutral24) \
  E_CPONLY(kColorRefNeutral25) \
  E_CPONLY(kColorRefNeutral30) \
  E_CPONLY(kColorRefNeutral40) \
  E_CPONLY(kColorRefNeutral50) \
  E_CPONLY(kColorRefNeutral60) \
  E_CPONLY(kColorRefNeutral70) \
  E_CPONLY(kColorRefNeutral80) \
  E_CPONLY(kColorRefNeutral87) \
  E_CPONLY(kColorRefNeutral90) \
  E_CPONLY(kColorRefNeutral92) \
  E_CPONLY(kColorRefNeutral94) \
  E_CPONLY(kColorRefNeutral95) \
  E_CPONLY(kColorRefNeutral96) \
  E_CPONLY(kColorRefNeutral98) \
  E_CPONLY(kColorRefNeutral99) \
  E_CPONLY(kColorRefNeutral100) \
  E_CPONLY(kColorRefNeutralVariant0) \
  E_CPONLY(kColorRefNeutralVariant10) \
  E_CPONLY(kColorRefNeutralVariant15) \
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
  /* UI material system color tokens. Id ordering matches UX design spec. */ \
  E_CPONLY(kColorSysPrimary) \
  E_CPONLY(kColorSysOnPrimary) \
  E_CPONLY(kColorSysPrimaryContainer) \
  E_CPONLY(kColorSysOnPrimaryContainer) \
  /* Secondary. */ \
  E_CPONLY(kColorSysSecondary) \
  E_CPONLY(kColorSysOnSecondary) \
  E_CPONLY(kColorSysSecondaryContainer) \
  E_CPONLY(kColorSysOnSecondaryContainer) \
  /* Tertiary. */ \
  E_CPONLY(kColorSysTertiary) \
  E_CPONLY(kColorSysOnTertiary) \
  E_CPONLY(kColorSysTertiaryContainer) \
  E_CPONLY(kColorSysOnTertiaryContainer) \
  /* Error. */ \
  E_CPONLY(kColorSysError) \
  E_CPONLY(kColorSysOnError) \
  E_CPONLY(kColorSysErrorContainer) \
  E_CPONLY(kColorSysOnErrorContainer) \
  /* Neutral. */ \
  E_CPONLY(kColorSysOnSurface) \
  E_CPONLY(kColorSysOnSurfaceVariant) \
  E_CPONLY(kColorSysOutline) \
  E_CPONLY(kColorSysSurfaceVariant) \
  /* Constant. */\
  E_CPONLY(kColorSysBlack) \
  E_CPONLY(kColorSysWhite) \
  /* Inverse. */ \
  E_CPONLY(kColorSysInversePrimary) \
  E_CPONLY(kColorSysInverseOnSurface) \
  E_CPONLY(kColorSysInverseSurface) \
  E_CPONLY(kColorSysInverseSurfacePrimary) \
  /* Surfaces. */ \
  E_CPONLY(kColorSysSurface) \
  E_CPONLY(kColorSysSurface1) \
  E_CPONLY(kColorSysSurface2) \
  E_CPONLY(kColorSysSurface3) \
  E_CPONLY(kColorSysSurface4) \
  E_CPONLY(kColorSysSurface5) \
  E_CPONLY(kColorSysSurfaceNumberedForeground) \
  /* General. */ \
  E_CPONLY(kColorSysOnSurfaceSecondary) \
  E_CPONLY(kColorSysOnSurfaceSubtle) \
  E_CPONLY(kColorSysOnSurfacePrimary) \
  E_CPONLY(kColorSysOnSurfacePrimaryInactive) \
  E_CPONLY(kColorSysTonalContainer) \
  E_CPONLY(kColorSysOnTonalContainer) \
  E_CPONLY(kColorSysBaseTonalContainer) \
  E_CPONLY(kColorSysOnBaseTonalContainer) \
  E_CPONLY(kColorSysTonalOutline) \
  E_CPONLY(kColorSysNeutralOutline) \
  E_CPONLY(kColorSysNeutralContainer) \
  E_CPONLY(kColorSysDivider) \
  /* Chrome surfaces. */ \
  E_CPONLY(kColorSysBase) \
  E_CPONLY(kColorSysBaseContainer) \
  E_CPONLY(kColorSysBaseContainerElevated) \
  E_CPONLY(kColorSysHeader) \
  E_CPONLY(kColorSysHeaderInactive) \
  E_CPONLY(kColorSysHeaderContainer) \
  E_CPONLY(kColorSysHeaderContainerInactive) \
  E_CPONLY(kColorSysOnHeaderDivider) \
  E_CPONLY(kColorSysOnHeaderDividerInactive) \
  E_CPONLY(kColorSysOnHeaderPrimary) \
  E_CPONLY(kColorSysOnHeaderPrimaryInactive) \
  /* States. */ \
  E_CPONLY(kColorSysStateHoverOnProminent) \
  E_CPONLY(kColorSysStateHoverOnSubtle) \
  E_CPONLY(kColorSysStateRippleNeutralOnProminent) \
  E_CPONLY(kColorSysStateRippleNeutralOnSubtle) \
  E_CPONLY(kColorSysStateRipplePrimary) \
  E_CPONLY(kColorSysStateFocusRing) \
  E_CPONLY(kColorSysStateTextHighlight) \
  E_CPONLY(kColorSysStateOnTextHighlight) \
  E_CPONLY(kColorSysStateFocusHighlight) \
  E_CPONLY(kColorSysStateDisabled) \
  E_CPONLY(kColorSysStateDisabledContainer) \
  E_CPONLY(kColorSysStateHoverDimBlendProtection) \
  E_CPONLY(kColorSysStateHoverBrightBlendProtection) \
  E_CPONLY(kColorSysStateInactiveRing) \
  E_CPONLY(kColorSysStateScrim) \
  E_CPONLY(kColorSysStateOnHeaderHover) \
  E_CPONLY(kColorSysStateHeaderHover) \
  E_CPONLY(kColorSysStateHeaderHoverInactive) \
  E_CPONLY(kColorSysStateHeaderSelect) \
  /* Effects. */ \
  E_CPONLY(kColorSysShadow) \
  E_CPONLY(kColorSysGradientPrimary) \
  E_CPONLY(kColorSysGradientTertiary) \
  /* AI. */ \
  E_CPONLY(kColorSysAiIllustrationShapeSurface1) \
  E_CPONLY(kColorSysAiIllustrationShapeSurface2) \
  E_CPONLY(kColorSysAiIllustrationShapeSurfaceGradientStart) \
  E_CPONLY(kColorSysAiIllustrationShapeSurfaceGradientEnd) \
  /* Experimentation. */ \
  E_CPONLY(kColorSysOmniboxContainer) \
  /* Deprecated */ \
  E_CPONLY(kColorSysStateHover) \
  E_CPONLY(kColorSysStateFocus) \
  E_CPONLY(kColorSysStatePressed) \
  /* Core color concepts */ \
  /* kColorAccent is used in color_provider_css_colors_test.ts. */ \
  /* If changing the variable name, the variable name in the test needs to */ \
  /* be changed as well. */ \
  E_CPONLY(kColorAccent) \
  E_CPONLY(kColorAccentWithGuaranteedContrastAtopPrimaryBackground) \
  E_CPONLY(kColorAlertHighSeverity) \
  E_CPONLY(kColorAlertLowSeverity) \
  E_CPONLY(kColorAlertMediumSeverityIcon) \
  E_CPONLY(kColorAlertMediumSeverityText) \
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
  E_CPONLY(kColorAppMenuProfileRowBackground) \
  E_CPONLY(kColorAppMenuProfileRowChipBackground) \
  E_CPONLY(kColorAppMenuProfileRowChipHovered) \
  E_CPONLY(kColorAppMenuRowBackgroundHovered) \
  E_CPONLY(kColorAppMenuUpgradeRowBackground) \
  E_CPONLY(kColorAppMenuUpgradeRowSubstringForeground) \
  E_CPONLY(kColorAvatarHeaderArt) \
  E_CPONLY(kColorAvatarIconGuest) \
  E_CPONLY(kColorAvatarIconIncognito) \
  E_CPONLY(kColorBadgeBackground) \
  E_CPONLY(kColorBadgeForeground) \
  E_CPONLY(kColorBadgeInCocoaMenuBackground) \
  E_CPONLY(kColorBadgeInCocoaMenuForeground) \
  E_CPONLY(kColorBubbleBackground) \
  E_CPONLY(kColorBubbleBorder) \
  E_CPONLY(kColorBubbleBorderShadowLarge) \
  E_CPONLY(kColorBubbleBorderShadowSmall) \
  E_CPONLY(kColorBubbleFooterBackground) \
  E_CPONLY(kColorBubbleFooterBorder) \
  E_CPONLY(kColorButtonFeatureAttentionHighlight) \
  E_CPONLY(kColorButtonBackground) \
  E_CPONLY(kColorButtonBackgroundPressed) \
  E_CPONLY(kColorButtonBackgroundProminent) \
  E_CPONLY(kColorButtonBackgroundProminentDisabled) \
  E_CPONLY(kColorButtonBackgroundProminentFocused) \
  E_CPONLY(kColorButtonBackgroundTonal) \
  E_CPONLY(kColorButtonBackgroundTonalDisabled) \
  E_CPONLY(kColorButtonBackgroundTonalFocused) \
  E_CPONLY(kColorButtonBackgroundWithAttention) \
  E_CPONLY(kColorButtonBorder) \
  E_CPONLY(kColorButtonBorderDisabled) \
  E_CPONLY(kColorButtonForeground) \
  E_CPONLY(kColorButtonForegroundDisabled) \
  E_CPONLY(kColorButtonForegroundProminent) \
  E_CPONLY(kColorButtonForegroundTonal) \
  E_CPONLY(kColorButtonHoverBackgroundText) \
  E_CPONLY(kColorMultitaskMenuNudgePulse) \
  E_CPONLY(kColorCheckboxCheck) \
  E_CPONLY(kColorCheckboxCheckDisabled) \
  E_CPONLY(kColorCheckboxContainer) \
  E_CPONLY(kColorCheckboxContainerDisabled) \
  E_CPONLY(kColorCheckboxOutline) \
  E_CPONLY(kColorCheckboxOutlineDisabled) \
  E_CPONLY(kColorCheckboxForegroundChecked) \
  E_CPONLY(kColorCheckboxForegroundUnchecked) \
  E_CPONLY(kColorChipBackgroundHover) \
  E_CPONLY(kColorChipBackgroundSelected) \
  E_CPONLY(kColorChipBorder) \
  E_CPONLY(kColorChipForeground) \
  E_CPONLY(kColorChipForegroundSelected) \
  E_CPONLY(kColorChipIcon) \
  E_CPONLY(kColorChipIconSelected) \
  E_CPONLY(kColorComboboxBackground) \
  E_CPONLY(kColorComboboxBackgroundDisabled) \
  E_CPONLY(kColorComboboxContainerOutline) \
  E_CPONLY(kColorComboboxInkDropHovered) \
  E_CPONLY(kColorComboboxInkDropRipple) \
  /* These colors correspond to the system colors defined in */ \
  /* ui::NativeTheme::SystemThemeColor. They are used to support */ \
  /* CSS system colors. */ \
  E_CPONLY(kColorCssSystemBtnFace) \
  E_CPONLY(kColorCssSystemBtnText) \
  E_CPONLY(kColorCssSystemGrayText) \
  E_CPONLY(kColorCssSystemHighlight) \
  E_CPONLY(kColorCssSystemHighlightText) \
  E_CPONLY(kColorCssSystemHotlight) \
  E_CPONLY(kColorCssSystemMenuHilight) \
  E_CPONLY(kColorCssSystemScrollbar) \
  E_CPONLY(kColorCssSystemWindow) \
  E_CPONLY(kColorCssSystemWindowText) \
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
  E_CPONLY(kColorFrameCaptionButtonUnfocused) \
  E_CPONLY(kColorFrameInactive) \
  E_CPONLY(kColorHelpIconActive) \
  E_CPONLY(kColorHelpIconInactive) \
  /* These should be refactored into chrome_color_id or removed once the */ \
  /* history clusters side panel is refactored to use shadow parts. */ \
  E_CPONLY(kColorHistoryClustersSidePanelDivider) \
  E_CPONLY(kColorHistoryClustersSidePanelDialogBackground) \
  E_CPONLY(kColorHistoryClustersSidePanelDialogDivider) \
  E_CPONLY(kColorHistoryClustersSidePanelDialogPrimaryForeground) \
  E_CPONLY(kColorHistoryClustersSidePanelDialogSecondaryForeground) \
  E_CPONLY(kColorHistoryClustersSidePanelCardSecondaryForeground) \
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
  E_CPONLY(kColorLinkForegroundDefault) \
  E_CPONLY(kColorLinkForegroundDisabled) \
  E_CPONLY(kColorLinkForegroundOnBubbleFooter) \
  E_CPONLY(kColorLinkForegroundPressed) \
  E_CPONLY(kColorLinkForegroundPressedDefault) \
  E_CPONLY(kColorLinkForegroundPressedOnBubbleFooter) \
  E_CPONLY(kColorListItemFolderIconBackground) \
  E_CPONLY(kColorListItemFolderIconForeground) \
  E_CPONLY(kColorListItemUrlFaviconBackground) \
  E_CPONLY(kColorLiveCaptionBubbleBackgroundDefault) \
  E_CPONLY(kColorLiveCaptionBubbleButtonIcon) \
  E_CPONLY(kColorLiveCaptionBubbleButtonIconDisabled) \
  E_CPONLY(kColorLiveCaptionBubbleForegroundDefault) \
  E_CPONLY(kColorLiveCaptionBubbleForegroundSecondary) \
  E_CPONLY(kColorLiveCaptionBubbleCheckbox) \
  E_CPONLY(kColorLiveCaptionBubbleLink) \
  E_CPONLY(kColorLoadingGradientBorder) \
  E_CPONLY(kColorLoadingGradientEnd) \
  E_CPONLY(kColorLoadingGradientMiddle) \
  E_CPONLY(kColorLoadingGradientStart) \
  E_CPONLY(kColorMenuBackground) \
  E_CPONLY(kColorMenuBorder) \
  E_CPONLY(kColorMenuButtonBackground) \
  E_CPONLY(kColorMenuButtonBackgroundSelected) \
  E_CPONLY(kColorMenuDropmarker) \
  E_CPONLY(kColorMenuIcon) \
  E_CPONLY(kColorMenuIconDisabled) \
  E_CPONLY(kColorMenuIconOnEmphasizedBackground) \
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
  E_CPONLY(kColorOverlayScrollbarFillHovered) \
  E_CPONLY(kColorOverlayScrollbarStroke) \
  E_CPONLY(kColorOverlayScrollbarStrokeHovered) \
  E_CPONLY(kColorProgressBar) \
  E_CPONLY(kColorProgressBarBackground) \
  E_CPONLY(kColorProgressBarPaused) \
  E_CPONLY(kColorRadioButtonForegroundUnchecked) \
  E_CPONLY(kColorRadioButtonForegroundDisabled) \
  E_CPONLY(kColorRadioButtonForegroundChecked) \
  E_CPONLY(kColorSegmentedButtonBorder) \
  E_CPONLY(kColorSegmentedButtonFocus) \
  E_CPONLY(kColorSegmentedButtonForegroundChecked) \
  E_CPONLY(kColorSegmentedButtonForegroundUnchecked) \
  E_CPONLY(kColorSegmentedButtonHover) \
  E_CPONLY(kColorSegmentedButtonRipple) \
  E_CPONLY(kColorSegmentedButtonChecked) \
  E_CPONLY(kColorSeparator) \
  E_CPONLY(kColorShadowBase) \
  E_CPONLY(kColorShadowValueAmbientShadowElevationFour) \
  E_CPONLY(kColorShadowValueAmbientShadowElevationSixteen) \
  E_CPONLY(kColorShadowValueAmbientShadowElevationThree) \
  E_CPONLY(kColorShadowValueAmbientShadowElevationTwelve) \
  E_CPONLY(kColorShadowValueAmbientShadowElevationTwentyFour) \
  E_CPONLY(kColorShadowValueKeyShadowElevationFour) \
  E_CPONLY(kColorShadowValueKeyShadowElevationSixteen) \
  E_CPONLY(kColorShadowValueKeyShadowElevationThree) \
  E_CPONLY(kColorShadowValueKeyShadowElevationTwelve) \
  E_CPONLY(kColorShadowValueKeyShadowElevationTwentyFour) \
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
  E_CPONLY(kColorSuggestionChipBorder) \
  E_CPONLY(kColorSuggestionChipIcon) \
  E_CPONLY(kColorTextfieldBackground) \
  E_CPONLY(kColorTextfieldBackgroundDisabled) \
  E_CPONLY(kColorTextfieldFilledBackground) \
  E_CPONLY(kColorTextfieldFilledForegroundInvalid) \
  E_CPONLY(kColorTextfieldFilledUnderline) \
  E_CPONLY(kColorTextfieldFilledUnderlineFocused) \
  E_CPONLY(kColorTextfieldForeground) \
  E_CPONLY(kColorTextfieldForegroundDisabled) \
  E_CPONLY(kColorTextfieldForegroundIcon) \
  E_CPONLY(kColorTextfieldForegroundLabel) \
  E_CPONLY(kColorTextfieldForegroundPlaceholderInvalid) \
  E_CPONLY(kColorTextfieldForegroundPlaceholder) \
  E_CPONLY(kColorTextfieldHover) \
  E_CPONLY(kColorTextfieldSelectionBackground) \
  E_CPONLY(kColorTextfieldSelectionForeground) \
  E_CPONLY(kColorTextfieldOutline) \
  E_CPONLY(kColorTextfieldOutlineDisabled) \
  E_CPONLY(kColorTextfieldOutlineInvalid) \
  E_CPONLY(kColorThemeColorPickerCheckmarkBackground) \
  E_CPONLY(kColorThemeColorPickerCheckmarkForeground) \
  E_CPONLY(kColorThemeColorPickerCustomColorIconBackground) \
  E_CPONLY(kColorThemeColorPickerHueSliderDialogBackground) \
  E_CPONLY(kColorThemeColorPickerHueSliderDialogForeground) \
  E_CPONLY(kColorThemeColorPickerHueSliderDialogIcon) \
  E_CPONLY(kColorThemeColorPickerHueSliderHandle) \
  E_CPONLY(kColorThemeColorPickerOptionBackground) \
  E_CPONLY(kColorThrobber) \
  E_CPONLY(kColorThrobberPreconnect) \
  E_CPONLY(kColorToastBackground) \
  E_CPONLY(kColorToastBackgroundProminent) \
  E_CPONLY(kColorToastButton) \
  E_CPONLY(kColorToastForeground) \
  E_CPONLY(kColorToggleButtonHover) \
  E_CPONLY(kColorToggleButtonPressed) \
  E_CPONLY(kColorToggleButtonShadow) \
  E_CPONLY(kColorToggleButtonThumbOff) \
  E_CPONLY(kColorToggleButtonThumbOffDisabled) \
  E_CPONLY(kColorToggleButtonThumbOn) \
  E_CPONLY(kColorToggleButtonThumbOnDisabled) \
  E_CPONLY(kColorToggleButtonThumbOnHover) \
  E_CPONLY(kColorToggleButtonTrackOff) \
  E_CPONLY(kColorToggleButtonTrackOffDisabled) \
  E_CPONLY(kColorToggleButtonTrackOn) \
  E_CPONLY(kColorToggleButtonTrackOnDisabled) \
  E_CPONLY(kColorToolbarSearchFieldBackground) \
  E_CPONLY(kColorToolbarSearchFieldBackgroundHover) \
  E_CPONLY(kColorToolbarSearchFieldBackgroundPressed) \
  E_CPONLY(kColorToolbarSearchFieldForeground) \
  E_CPONLY(kColorToolbarSearchFieldForegroundPlaceholder) \
  E_CPONLY(kColorToolbarSearchFieldIcon) \
  E_CPONLY(kColorTooltipBackground) \
  E_CPONLY(kColorTooltipForeground) \
  E_CPONLY(kColorTreeBackground) \
  E_CPONLY(kColorTreeNodeBackgroundSelectedFocused) \
  E_CPONLY(kColorTreeNodeBackgroundSelectedUnfocused) \
  E_CPONLY(kColorTreeNodeForeground) \
  E_CPONLY(kColorTreeNodeForegroundSelectedFocused) \
  E_CPONLY(kColorTreeNodeForegroundSelectedUnfocused) \
  /* These colors are used to paint the controls defined in */ \
  /* ui::NativeThemeBase::ControlColorId. */ \
  E_CPONLY(kColorWebNativeControlAccent) \
  E_CPONLY(kColorWebNativeControlAccentDisabled) \
  E_CPONLY(kColorWebNativeControlAccentHovered) \
  E_CPONLY(kColorWebNativeControlAccentPressed) \
  E_CPONLY(kColorWebNativeControlAutoCompleteBackground) \
  E_CPONLY(kColorWebNativeControlBackground) \
  E_CPONLY(kColorWebNativeControlBackgroundDisabled) \
  E_CPONLY(kColorWebNativeControlBorder) \
  E_CPONLY(kColorWebNativeControlBorderDisabled) \
  E_CPONLY(kColorWebNativeControlBorderHovered) \
  E_CPONLY(kColorWebNativeControlBorderPressed) \
  E_CPONLY(kColorWebNativeControlButtonBorder) \
  E_CPONLY(kColorWebNativeControlButtonBorderDisabled) \
  E_CPONLY(kColorWebNativeControlButtonBorderHovered) \
  E_CPONLY(kColorWebNativeControlButtonBorderPressed) \
  E_CPONLY(kColorWebNativeControlButtonFill) \
  E_CPONLY(kColorWebNativeControlButtonFillDisabled) \
  E_CPONLY(kColorWebNativeControlButtonFillHovered) \
  E_CPONLY(kColorWebNativeControlButtonFillPressed) \
  E_CPONLY(kColorWebNativeControlFill) \
  E_CPONLY(kColorWebNativeControlFillDisabled) \
  E_CPONLY(kColorWebNativeControlFillHovered) \
  E_CPONLY(kColorWebNativeControlFillPressed) \
  E_CPONLY(kColorWebNativeControlLightenLayer) \
  E_CPONLY(kColorWebNativeControlProgressValue) \
  E_CPONLY(kColorWebNativeControlScrollbarArrowBackgroundHovered) \
  E_CPONLY(kColorWebNativeControlScrollbarArrowBackgroundPressed) \
  E_CPONLY(kColorWebNativeControlScrollbarArrowForeground) \
  E_CPONLY(kColorWebNativeControlScrollbarArrowForegroundPressed) \
  E_CPONLY(kColorWebNativeControlScrollbarCorner) \
  E_CPONLY(kColorWebNativeControlScrollbarThumb) \
  E_CPONLY(kColorWebNativeControlScrollbarThumbHovered) \
  E_CPONLY(kColorWebNativeControlScrollbarThumbInactive) \
  E_CPONLY(kColorWebNativeControlScrollbarThumbOverlayMinimalMode) \
  E_CPONLY(kColorWebNativeControlScrollbarThumbPressed) \
  E_CPONLY(kColorWebNativeControlScrollbarTrack) \
  E_CPONLY(kColorWebNativeControlSlider) \
  E_CPONLY(kColorWebNativeControlSliderDisabled) \
  E_CPONLY(kColorWebNativeControlSliderHovered) \
  E_CPONLY(kColorWebNativeControlSliderPressed) \
  E_CPONLY(kColorWindowBackground)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#define CHROMEOS_ASH_COLOR_IDS \
  /* Colors for illustrations */ \
  E_CPONLY(kColorNativeColor1) \
  E_CPONLY(kColorNativeColor1Shade1) \
  E_CPONLY(kColorNativeColor1Shade2) \
  E_CPONLY(kColorNativeColor2) \
  E_CPONLY(kColorNativeColor3) \
  E_CPONLY(kColorNativeColor4) \
  E_CPONLY(kColorNativeColor5) \
  E_CPONLY(kColorNativeColor6) \
  E_CPONLY(kColorNativeBaseColor) \
  E_CPONLY(kColorNativeSecondaryColor) \
  E_CPONLY(kColorNativeOnPrimaryContainerColor) \
  E_CPONLY(kColorNativeAnalogColor) \
  E_CPONLY(kColorNativeMutedColor) \
  E_CPONLY(kColorNativeComplementColor) \
  E_CPONLY(kColorNativeOnGradientColor)
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#define CHROMEOS_ASH_COLOR_IDS
#endif
#if BUILDFLAG(IS_CHROMEOS)
#define PLATFORM_SPECIFIC_COLOR_IDS \
  CHROMEOS_ASH_COLOR_IDS \
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
  E_CPONLY(kColorAshInputOverlayFocusRing) \
  E_CPONLY(kColorAshIconInOobe) \
  \
  /* TODO(crbug/1319917): Remove these when dark light mode is launched. */ \
  E_CPONLY(kColorAshLightFocusRing) \
  \
  E_CPONLY(kColorAshOnboardingFocusRing) \
  \
  E_CPONLY(kColorAshPrivacyIndicatorsBackground) \
  \
  E_CPONLY(kColorAshSystemUIMenuBackground) \
  E_CPONLY(kColorAshSystemUIMenuIcon) \
  E_CPONLY(kColorAshSystemUIMenuItemBackgroundSelected) \
  E_CPONLY(kColorAshSystemUIMenuSeparator) \
  \
  /* TODO(b/291622042): Delete these colors when Jelly is launched */ \
  E_CPONLY(kColorHighlightBorderBorder1) \
  E_CPONLY(kColorHighlightBorderBorder2) \
  E_CPONLY(kColorHighlightBorderBorder3) \
  E_CPONLY(kColorHighlightBorderHighlight1) \
  E_CPONLY(kColorHighlightBorderHighlight2) \
  E_CPONLY(kColorHighlightBorderHighlight3) \
  \
  E_CPONLY(kColorCrosSystemHighlight) \
  E_CPONLY(kColorCrosSystemHighlightBorder) \
  E_CPONLY(kColorCrosSystemHighlightBorder1) \
  \
  E_CPONLY(kColorCrosSysPositive) \
  E_CPONLY(kColorCrosSysComplementVariant)
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
