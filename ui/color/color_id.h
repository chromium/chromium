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
  E_CPONLY(kColorAccent, kUiColorsStart) \
  E(kColorAlertHighSeverity, NativeTheme::kColorId_AlertSeverityHigh) \
  E(kColorAlertLowSeverity, NativeTheme::kColorId_AlertSeverityLow) \
  E(kColorAlertMediumSeverity, NativeTheme::kColorId_AlertSeverityMedium) \
  E_CPONLY(kColorDisabledForeground) \
  E_CPONLY(kColorItemSelectionBackground) \
  E_CPONLY(kColorMidground) \
  E_CPONLY(kColorPrimaryBackground) \
  E_CPONLY(kColorPrimaryForeground) \
  E_CPONLY(kColorSecondaryForeground) \
  E_CPONLY(kColorSubtleEmphasisBackground) \
  E_CPONLY(kColorTextSelectionBackground) \
  \
  /* Further UI element colors */ \
  E(kColorAvatarHeaderArt, NativeTheme::kColorId_AvatarHeaderArt) \
  E(kColorAvatarIconGuest, NativeTheme::kColorId_AvatarIconGuest) \
  E(kColorAvatarIconIncognito, NativeTheme::kColorId_AvatarIconIncognito) \
  E(kColorBubbleBackground, NativeTheme::kColorId_BubbleBackground) \
  E(kColorBubbleFooterBackground, \
    NativeTheme::kColorId_BubbleFooterBackground) \
  E(kColorButtonBackground, NativeTheme::kColorId_ButtonColor) \
  /* TODO(https://crbug.com/1003612): Map this to old color id. */ \
  E_CPONLY(kColorButtonBackgroundPressed) \
  E(kColorButtonBackgroundProminent, \
    NativeTheme::kColorId_ProminentButtonColor) \
  E(kColorButtonBackgroundProminentDisabled, \
    NativeTheme::kColorId_ProminentButtonDisabledColor) \
  E(kColorButtonBackgroundProminentFocused, \
    NativeTheme::kColorId_ProminentButtonFocusedColor) \
  E(kColorButtonBorder, NativeTheme::kColorId_ButtonBorderColor) \
  E(kColorButtonBorderDisabled, \
    NativeTheme::kColorId_DisabledButtonBorderColor) \
  E(kColorButtonForeground, NativeTheme::kColorId_ButtonEnabledColor) \
  E(kColorButtonForegroundDisabled, NativeTheme::kColorId_ButtonDisabledColor) \
  E(kColorButtonForegroundProminent, \
    NativeTheme::kColorId_TextOnProminentButtonColor) \
  E(kColorButtonForegroundUnchecked, \
    NativeTheme::kColorId_ButtonUncheckedColor) \
  E(kColorDialogBackground, NativeTheme::kColorId_DialogBackground) \
  E(kColorDialogForeground, NativeTheme::kColorId_DialogForeground) \
  E(kColorFocusableBorderFocused, NativeTheme::kColorId_FocusedBorderColor) \
  E(kColorFocusableBorderUnfocused, \
    NativeTheme::kColorId_UnfocusedBorderColor) \
  E(kColorIcon, NativeTheme::kColorId_DefaultIconColor) \
  E(kColorLabelForeground, NativeTheme::kColorId_LabelEnabledColor) \
  E(kColorLabelForegroundDisabled, NativeTheme::kColorId_LabelDisabledColor) \
  E(kColorLabelForegroundSecondary, NativeTheme::kColorId_LabelSecondaryColor) \
  E(kColorLabelSelectionBackground, \
    NativeTheme::kColorId_LabelTextSelectionBackgroundFocused) \
  E(kColorLabelSelectionForeground, \
    NativeTheme::kColorId_LabelTextSelectionColor) \
  E(kColorLinkForeground, NativeTheme::kColorId_LinkEnabled) \
  E(kColorLinkForegroundDisabled, NativeTheme::kColorId_LinkDisabled) \
  E(kColorLinkForegroundPressed, NativeTheme::kColorId_LinkPressed) \
  E(kColorMenuBackground, NativeTheme::kColorId_MenuBackgroundColor) \
  E(kColorMenuBorder, NativeTheme::kColorId_MenuBorderColor) \
  E(kColorMenuIcon, NativeTheme::kColorId_MenuIconColor) \
  E(kColorMenuItemBackgroundAlertedInitial, \
    NativeTheme::kColorId_MenuItemInitialAlertBackgroundColor) \
  E(kColorMenuItemBackgroundAlertedTarget, \
    NativeTheme::kColorId_MenuItemTargetAlertBackgroundColor) \
  E(kColorMenuItemBackgroundHighlighted, \
    NativeTheme::kColorId_HighlightedMenuItemBackgroundColor) \
  E(kColorMenuItemBackgroundSelected, \
    NativeTheme::kColorId_FocusedMenuItemBackgroundColor) \
  E(kColorMenuItemForeground, \
    NativeTheme::kColorId_EnabledMenuItemForegroundColor) \
  E(kColorMenuItemForegroundDisabled, \
    NativeTheme::kColorId_DisabledMenuItemForegroundColor) \
  E(kColorMenuItemForegroundHighlighted, \
    NativeTheme::kColorId_HighlightedMenuItemForegroundColor) \
  E(kColorMenuItemForegroundSecondary, \
    NativeTheme::kColorId_MenuItemMinorTextColor) \
  E(kColorMenuItemForegroundSelected, \
    NativeTheme::kColorId_SelectedMenuItemForegroundColor) \
  E(kColorMenuSeparator, NativeTheme::kColorId_MenuSeparatorColor) \
  E(kColorTabBorderSelected, NativeTheme::kColorId_TabSelectedBorderColor) \
  E(kColorTabContentSeparator, NativeTheme::kColorId_TabBottomBorder) \
  E(kColorTabForeground, NativeTheme::kColorId_TabTitleColorInactive) \
  E(kColorTabForegroundSelected, \
    NativeTheme::kColorId_TabTitleColorActive) \
  E(kColorTableBackground, NativeTheme::kColorId_TableBackground) \
  E(kColorTableBackgroundSelectedFocused, \
    NativeTheme::kColorId_TableSelectionBackgroundFocused) \
  E(kColorTableBackgroundSelectedUnfocused, \
    NativeTheme::kColorId_TableSelectionBackgroundUnfocused) \
  E(kColorTableForeground, NativeTheme::kColorId_TableText) \
  E(kColorTableForegroundSelectedFocused, \
    NativeTheme::kColorId_TableSelectedText) \
  E(kColorTableForegroundSelectedUnfocused, \
    NativeTheme::kColorId_TableSelectedTextUnfocused) \
  E(kColorTableGroupingIndicator, \
    NativeTheme::kColorId_TableGroupingIndicatorColor) \
  E(kColorTableHeaderBackground, NativeTheme::kColorId_TableHeaderBackground) \
  E(kColorTableHeaderForeground, NativeTheme::kColorId_TableHeaderText) \
  E(kColorTableHeaderSeparator, NativeTheme::kColorId_TableHeaderSeparator) \
  E(kColorTextfieldBackground, \
    NativeTheme::kColorId_TextfieldDefaultBackground) \
  E(kColorTextfieldBackgroundDisabled, \
    NativeTheme::kColorId_TextfieldReadOnlyBackground) \
  E(kColorTextfieldForeground, NativeTheme::kColorId_TextfieldDefaultColor) \
  E(kColorTextfieldForegroundDisabled, \
    NativeTheme::kColorId_TextfieldReadOnlyColor) \
  E(kColorTextfieldForegroundPlaceholder, \
    NativeTheme::kColorId_TextfieldPlaceholderColor) \
  E(kColorTextfieldSelectionBackground, \
    NativeTheme::kColorId_TextfieldSelectionBackgroundFocused) \
  E(kColorTextfieldSelectionForeground, \
    NativeTheme::kColorId_TextfieldSelectionColor) \
  E(kColorThrobber, NativeTheme::kColorId_ThrobberSpinningColor) \
  E(kColorTooltipBackground, NativeTheme::kColorId_TooltipBackground) \
  E(kColorTooltipForeground, NativeTheme::kColorId_TooltipText) \
  E(kColorTreeBackground, NativeTheme::kColorId_TreeBackground) \
  E(kColorTreeNodeBackgroundSelectedFocused, \
    NativeTheme::kColorId_TreeSelectionBackgroundFocused) \
  E(kColorTreeNodeBackgroundSelectedUnfocused, \
    NativeTheme::kColorId_TreeSelectionBackgroundUnfocused) \
  E(kColorTreeNodeForeground, NativeTheme::kColorId_TreeText) \
  E(kColorTreeNodeForegroundSelectedFocused, \
    NativeTheme::kColorId_TreeSelectedText) \
  E(kColorTreeNodeForegroundSelectedUnfocused, \
    NativeTheme::kColorId_TreeSelectedTextUnfocused) \
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

#if defined(OS_APPLE)
#define MACOSX_COLOR_IDS \
  E(kColorTableBackgroundAlternate, \
    NativeTheme::kColorId_TableBackgroundAlternate)
#endif

#if defined(OS_WIN)
#define COLOR_IDS \
  CROSS_PLATFORM_COLOR_IDS \
  WIN_COLOR_IDS
#elif defined(OS_APPLE)
#define COLOR_IDS \
  CROSS_PLATFORM_COLOR_IDS \
  MACOSX_COLOR_IDS
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
