// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#include <windows.h>

#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_set.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_utils.h"

namespace ui {

void AddNativeCoreColorMixer(ColorProvider* provider,
                             bool dark_window,
                             bool high_contrast) {
  // TODO(pkasting): Not clear whether this is really the set of interest.
  // Maybe there's some way to query colors used by UxTheme.dll, or maybe we
  // should be hardcoding a list of colors for system light/dark modes based on
  // reverse-engineering current Windows behavior.  Or maybe the union of all
  // these.
#define MAP(chrome, native) {chrome, color_utils::GetSysSkColor(native)}
  ColorMixer& mixer = provider->AddMixer();

  if (!high_contrast)
    return;

  mixer.AddSet(
      {kColorSetNative,
       {
           MAP(kColorNative3dDkShadow, COLOR_3DDKSHADOW),
           MAP(kColorNative3dLight, COLOR_3DLIGHT),
           MAP(kColorNativeActiveBorder, COLOR_ACTIVEBORDER),
           MAP(kColorNativeActiveCaption, COLOR_ACTIVECAPTION),
           MAP(kColorNativeAppWorkspace, COLOR_APPWORKSPACE),
           MAP(kColorNativeBackground, COLOR_BACKGROUND),
           MAP(kColorNativeBtnFace, COLOR_BTNFACE),
           MAP(kColorNativeBtnHighlight, COLOR_BTNHIGHLIGHT),
           MAP(kColorNativeBtnShadow, COLOR_BTNSHADOW),
           MAP(kColorNativeBtnText, COLOR_BTNTEXT),
           MAP(kColorNativeCaptionText, COLOR_CAPTIONTEXT),
           MAP(kColorNativeGradientActiveCaption, COLOR_GRADIENTACTIVECAPTION),
           MAP(kColorNativeGradientInactiveCaption,
               COLOR_GRADIENTINACTIVECAPTION),
           MAP(kColorNativeGrayText, COLOR_GRAYTEXT),
           MAP(kColorNativeHighlight, COLOR_HIGHLIGHT),
           MAP(kColorNativeHighlightText, COLOR_HIGHLIGHTTEXT),
           MAP(kColorNativeHotlight, COLOR_HOTLIGHT),
           MAP(kColorNativeInactiveBorder, COLOR_INACTIVEBORDER),
           MAP(kColorNativeInactiveCaption, COLOR_INACTIVECAPTION),
           MAP(kColorNativeInactiveCaptionText, COLOR_INACTIVECAPTIONTEXT),
           MAP(kColorNativeInfoBk, COLOR_INFOBK),
           MAP(kColorNativeInfoText, COLOR_INFOTEXT),
           MAP(kColorNativeMenu, COLOR_MENU),
           MAP(kColorNativeMenuBar, COLOR_MENUBAR),
           MAP(kColorNativeMenuHilight, COLOR_MENUHILIGHT),
           MAP(kColorNativeMenuText, COLOR_MENUTEXT),
           MAP(kColorNativeScrollbar, COLOR_SCROLLBAR),
           MAP(kColorNativeWindow, COLOR_WINDOW),
           MAP(kColorNativeWindowFrame, COLOR_WINDOWFRAME),
           MAP(kColorNativeWindowText, COLOR_WINDOWTEXT),
       }});

  // Window Background
  mixer[kColorPrimaryBackground] = {kColorNativeWindow};
  mixer[kColorWindowBackground] = {kColorNativeWindow};
  mixer[kColorButtonBackgroundProminentDisabled] = {kColorNativeWindow};

  // Window Text
  mixer[kColorAlertLowSeverity] = {kColorNativeWindowText};
  mixer[kColorAlertMediumSeverity] = {kColorNativeWindowText};
  mixer[kColorAlertHighSeverity] = {kColorNativeWindowText};
  mixer[kColorIcon] = {kColorNativeWindowText};
  mixer[kColorMidground] = {kColorNativeWindowText};
  mixer[kColorPrimaryForeground] = {kColorNativeWindowText};
  mixer[kColorSecondaryForeground] = {kColorNativeWindowText};
  mixer[kColorTableGroupingIndicator] = {kColorNativeWindowText};
  mixer[kColorTooltipForeground] = {kColorNativeWindowText};
  mixer[kColorThrobber] = {kColorNativeWindowText};

  // Hyperlinks
  mixer[kColorLinkForeground] = {kColorNativeHotlight};
  mixer[kColorLinkForegroundPressed] = {kColorNativeHotlight};
  mixer[kColorMenuItemForegroundHighlighted] = {kColorNativeHotlight};

  // Gray/Disabled Text
  mixer[kColorMenuItemForegroundDisabled] = {kColorNativeGrayText};
  mixer[kColorLinkForegroundDisabled] = {kColorNativeGrayText};
  mixer[kColorLabelForegroundDisabled] = {kColorNativeGrayText};
  mixer[kColorButtonForegroundDisabled] = {kColorNativeGrayText};

  // Button Background
  mixer[kColorButtonBackground] = {kColorNativeBtnFace};
  mixer[kColorMenuBackground] = {kColorNativeBtnFace};
  mixer[kColorTextfieldBackground] = {kColorNativeBtnFace};
  mixer[kColorTextfieldBackgroundDisabled] = {kColorNativeBtnFace};
  mixer[kColorSubtleEmphasisBackground] = {kColorNativeBtnFace};

  // Button Text Foreground
  mixer[kColorMenuItemForeground] = {kColorNativeBtnText};
  mixer[kColorMenuItemForegroundSecondary] = {kColorNativeBtnText};
  mixer[kColorMenuBorder] = {kColorNativeBtnText};
  mixer[kColorMenuSeparator] = {kColorNativeBtnText};
  mixer[kColorTextfieldForeground] = {kColorNativeBtnText};
  mixer[kColorButtonForeground] = {kColorNativeBtnText};
  mixer[kColorFocusableBorderUnfocused] = {kColorNativeBtnText};
  mixer[kColorTextfieldForegroundPlaceholder] = {kColorNativeBtnText};
  mixer[kColorTextfieldForegroundDisabled] = {kColorNativeBtnText};
  mixer[kColorFocusableBorderFocused] = {kColorNativeBtnText};
  mixer[kColorTabForegroundSelected] = {kColorNativeBtnText};
  mixer[kColorTabForeground] = {kColorNativeBtnText};
  mixer[kColorTabContentSeparator] = {kColorNativeBtnText};

  // Highlight/Selected Background
  mixer[kColorAccent] = {kColorNativeHighlight};
  mixer[kColorTextSelectionBackground] = {kColorNativeHighlight};
  mixer[kColorButtonBackgroundProminent] = {kColorNativeHighlight};
  mixer[kColorButtonBackgroundProminentFocused] = {kColorNativeHighlight};
  mixer[kColorButtonBorder] = {kColorNativeHighlight};
  mixer[kColorMenuItemBackgroundSelected] = {kColorNativeHighlight};
  mixer[kColorLabelSelectionBackground] = {kColorNativeHighlight};
  mixer[kColorTextfieldSelectionBackground] = {kColorNativeHighlight};
  mixer[kColorTreeNodeBackgroundSelectedFocused] = {kColorNativeHighlight};
  mixer[kColorTreeNodeBackgroundSelectedUnfocused] = {kColorNativeHighlight};
  mixer[kColorTableBackgroundSelectedFocused] = {kColorNativeHighlight};
  mixer[kColorTableBackgroundSelectedUnfocused] = {kColorNativeHighlight};

  // Highlight/Selected Text Foreground
  mixer[kColorButtonForegroundProminent] = {kColorNativeHighlightText};
  mixer[kColorMenuItemForegroundSelected] = {kColorNativeHighlightText};
  mixer[kColorTextfieldSelectionForeground] = {kColorNativeHighlightText};
  mixer[kColorLabelSelectionForeground] = {kColorNativeHighlightText};
  mixer[kColorTreeNodeForegroundSelectedFocused] = {kColorNativeHighlightText};
  mixer[kColorTreeNodeForegroundSelectedUnfocused] = {
      kColorNativeHighlightText};
  mixer[kColorTableForegroundSelectedFocused] = {kColorNativeHighlightText};
  mixer[kColorTableForegroundSelectedUnfocused] = {kColorNativeHighlightText};
}

void AddNativeUiColorMixer(ColorProvider* provider,
                           bool dark_window,
                           bool high_contrast) {
  // TODO(pkasting): Add recipes
}

void AddNativePostprocessingMixer(ColorProvider* provider) {}

}  // namespace ui
