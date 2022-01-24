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
#include "ui/gfx/color_palette.h"
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
  ColorMixer& mixer = provider->AddMixer();

  if (!high_contrast)
    return;

#define E(chrome, native) {chrome, color_utils::GetSysSkColor(native)},
  mixer.AddSet({kColorSetNative, {WIN_COLOR_IDS}});
#undef E

  // Window Background
  mixer[kColorPrimaryBackground] = {kColorNativeWindow};

  // Window Text
  mixer[kColorAlertLowSeverity] = {kColorNativeWindowText};
  mixer[kColorAlertMediumSeverity] = {kColorNativeWindowText};
  mixer[kColorAlertHighSeverity] = {kColorNativeWindowText};
  mixer[kColorIcon] = {kColorNativeWindowText};
  mixer[kColorMidground] = {kColorNativeWindowText};
  mixer[kColorPrimaryForeground] = {kColorNativeWindowText};
  mixer[kColorSecondaryForeground] = {kColorNativeWindowText};

  // Gray/Disabled Text
  mixer[kColorDisabledForeground] = {kColorNativeGrayText};

  // Button Background
  mixer[kColorSubtleEmphasisBackground] = {kColorNativeBtnFace};

  // Button Text Foreground
  mixer[kColorMenuItemForeground] = {kColorNativeBtnText};

  // Highlight/Selected Background
  mixer[kColorAccent] = {kColorNativeHighlight};
  mixer[kColorItemSelectionBackground] = {kColorNativeHighlight};
  mixer[kColorMenuSelectionBackground] = {kColorNativeHighlight};
  mixer[kColorSubtleAccent] = {kColorNativeHighlight};
  mixer[kColorTextSelectionBackground] = {kColorNativeHighlight};

  // Highlight/Selected Text Foreground
  mixer[kColorTextSelectionForeground] = {kColorNativeHighlightText};
}

void AddNativeUiColorMixer(ColorProvider* provider,
                           bool dark_window,
                           bool high_contrast) {
  if (!high_contrast)
    return;

  ColorMixer& mixer = provider->AddMixer();

  mixer[kColorButtonForegroundChecked] = {dark_window ? gfx::kGoogleBlue100
                                                      : gfx::kGoogleBlue900};
  mixer[kColorNotificationInputPlaceholderForeground] =
      SetAlpha(kColorNotificationInputForeground, gfx::kGoogleGreyAlpha700);
  mixer[kColorSliderTrack] = AlphaBlend(
      kColorNativeHighlight, kColorNativeWindow, gfx::kGoogleGreyAlpha400);

  // Window Background
  mixer[kColorBubbleFooterBackground] = {kColorNativeWindow};
  mixer[kColorTooltipBackground] = {kColorNativeWindow};
  mixer[kColorButtonBackgroundProminentDisabled] = {kColorNativeWindow};

  // Window Text
  mixer[kColorTableGroupingIndicator] = {kColorNativeWindowText};
  mixer[kColorThrobber] = {kColorNativeWindowText};
  mixer[kColorTooltipForeground] = {kColorNativeWindowText};

  // Hyperlinks
  mixer[kColorLinkForeground] = {kColorNativeHotlight};
  mixer[kColorLinkForegroundPressed] = {kColorNativeHotlight};
  mixer[kColorMenuItemForegroundHighlighted] = {kColorNativeHotlight};

  // Gray/Disabled Text
  mixer[kColorMenuItemForegroundDisabled] = {kColorNativeGrayText};
  mixer[kColorLinkForegroundDisabled] = {kColorNativeGrayText};
  mixer[kColorLabelForegroundDisabled] = {kColorNativeGrayText};
  mixer[kColorButtonForegroundDisabled] = {kColorNativeGrayText};
  mixer[kColorThrobberPreconnect] = {kColorNativeGrayText};

  // Button Background
  mixer[kColorButtonBackground] = {kColorNativeBtnFace};
  mixer[kColorMenuBackground] = {kColorNativeBtnFace};
  mixer[kColorTextfieldBackground] = {kColorNativeBtnFace};
  mixer[kColorTextfieldBackgroundDisabled] = {kColorNativeBtnFace};

  // Button Text Foreground
  mixer[kColorButtonForeground] = {kColorNativeBtnText};
  mixer[kColorFocusableBorderFocused] = {kColorNativeBtnText};
  mixer[kColorFocusableBorderUnfocused] = {kColorNativeBtnText};
  mixer[kColorMenuBorder] = {kColorNativeBtnText};
  mixer[kColorMenuItemForegroundSecondary] = {kColorNativeBtnText};
  mixer[kColorMenuSeparator] = {kColorNativeBtnText};
  mixer[kColorSeparator] = {kColorNativeBtnText};
  mixer[kColorTabContentSeparator] = {kColorNativeBtnText};
  mixer[kColorTabForeground] = {kColorNativeBtnText};
  mixer[kColorTabForegroundSelected] = {kColorNativeBtnText};
  mixer[kColorTextfieldForeground] = {kColorNativeBtnText};
  mixer[kColorTextfieldForegroundPlaceholder] = {kColorNativeBtnText};
  mixer[kColorTextfieldForegroundDisabled] = {kColorNativeBtnText};

  // Highlight/Selected Background
  mixer[kColorButtonBorder] = {kColorNativeHighlight};
  mixer[kColorButtonBackgroundProminentFocused] = {kColorNativeHighlight};
  mixer[kColorHelpIconActive] = {kColorNativeHighlight};

  // Highlight/Selected Text Foreground
  mixer[kColorButtonForegroundProminent] = {kColorNativeHighlightText};
  mixer[kColorMenuItemForegroundSelected] = {kColorNativeHighlightText};
  mixer[kColorNotificationInputForeground] = {kColorNativeHighlightText};
  mixer[kColorTableForegroundSelectedFocused] = {kColorNativeHighlightText};
  mixer[kColorTableForegroundSelectedUnfocused] = {kColorNativeHighlightText};
  mixer[kColorTreeNodeForegroundSelectedFocused] = {kColorNativeHighlightText};
  mixer[kColorTreeNodeForegroundSelectedUnfocused] = {
      kColorNativeHighlightText};
}

}  // namespace ui
