// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_ID_H_
#define UI_COLOR_COLOR_ID_H_

#include "build/build_config.h"

namespace ui {

// ColorId contains identifiers for all input, intermediary, and output colors
// known to the core UI layer.  Embedders can extend this enum with additional
// values that are understood by the ColorProvider implementation.  Embedders
// define enum values from kUiColorsEnd.  Values named beginning with "kColor"
// represent the actual colors; the rest are markers.
using ColorId = int;
enum ColorIds : ColorId {
  kUiColorsStart = 0,

  // Core color concepts
  kColorAccent = kUiColorsStart,
  kColorAlertHighSeverity,
  kColorAlertLowSeverity,
  kColorAlertMediumSeverity,
  kColorBodyForeground,
  kColorLinkForeground,
  kColorPrimaryBackground,
  kColorPrimaryForeground,
  kColorSecondaryBackground,
  kColorSecondaryBackgroundSubtle,
  kColorSecondaryForeground,
  kColorSeparatorForeground,
  kColorTextSelectionBackground,

  // Further UI element colors
  kColorBubbleBackground,
  kColorBubbleFooterBackground,
  kColorButtonBackground,
  kColorButtonBorder,
  kColorButtonDisabledForeground,
  kColorButtonForeground,
  kColorButtonProminentBackground,
  kColorButtonProminentDisabledBackground,
  kColorButtonProminentFocusedBackground,
  kColorButtonProminentForeground,
  kColorDialogBackground,
  kColorDialogForeground,
  kColorFocusableBorderFocused,
  kColorFocusableBorderUnfocused,
  kColorIcon,
  kColorLabelDisabledForeground,
  kColorLabelForeground,
  kColorLabelSelectionBackground,
  kColorLabelSelectionForeground,
  kColorLinkDisabledForeground,
  kColorLinkPressedForeground,
  kColorMenuBackground,
  kColorMenuBorder,
  kColorMenuItemAlertedBackground,
  kColorMenuItemDisabledForeground,
  kColorMenuItemForeground,
  kColorMenuItemHighlightedBackground,
  kColorMenuItemHighlightedForeground,
  kColorMenuItemSecondaryForeground,
  kColorMenuItemSelectedBackground,
  kColorMenuItemSelectedForeground,
  kColorMenuSeparator,
  kColorTabContentSeparator,
  kColorTabForeground,
  kColorTabSelectedForeground,
  kColorTableBackground,
  kColorTableForeground,
  kColorTableGroupingIndicator,
  kColorTableHeaderBackground,
  kColorTableHeaderForeground,
  kColorTableHeaderSeparator,
  kColorTableSelectedFocusedBackground,
  kColorTableSelectedFocusedForeground,
  kColorTableSelectedUnfocusedBackground,
  kColorTableSelectedUnfocusedForeground,
  kColorTextfieldBackground,
  kColorTextfieldDisabledBackground,
  kColorTextfieldDisabledForeground,
  kColorTextfieldForeground,
  kColorTextfieldSelectionBackground,
  kColorTextfieldSelectionForeground,
  kColorThrobber,
  kColorTooltipBackground,
  kColorTooltipForeground,
  kColorTreeBackground,
  kColorTreeNodeForeground,
  kColorTreeNodeSelectedFocusedBackground,
  kColorTreeNodeSelectedFocusedForeground,
  kColorTreeNodeSelectedUnfocusedBackground,
  kColorTreeNodeSelectedUnfocusedForeground,
  kColorWindowBackground,

#if defined(OS_WIN)
  // Windows native colors
  kColorNative3dDkShadow,
  kColorNative3dLight,
  kColorNativeActiveBorder,
  kColorNativeActiveCaption,
  kColorNativeAppWorkspace,
  kColorNativeBackground,
  kColorNativeBtnFace,
  kColorNativeBtnHighlight,
  kColorNativeBtnShadow,
  kColorNativeBtnText,
  kColorNativeCaptionText,
  kColorNativeGradientActiveCaption,
  kColorNativeGradientInactiveCaption,
  kColorNativeGrayText,
  kColorNativeHighlight,
  kColorNativeHighlightText,
  kColorNativeHotlight,
  kColorNativeInactiveBorder,
  kColorNativeInactiveCaption,
  kColorNativeInactiveCaptionText,
  kColorNativeInfoBk,
  kColorNativeInfoText,
  kColorNativeMenu,
  kColorNativeMenuBar,
  kColorNativeMenuHilight,
  kColorNativeMenuText,
  kColorNativeScrollbar,
  kColorNativeWindow,
  kColorNativeWindowFrame,
  kColorNativeWindowText,
#endif  // defined(OS_WIN)

  // TODO(pkasting): Other native colors

  // Embedders must start color IDs from this value.
  kUiColorsEnd,

  // Embedders must not assign IDs larger than this value.  This is used to
  // verify that color IDs and color set IDs are not interchanged.
  kUiColorsLast = 0xffff
};

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
