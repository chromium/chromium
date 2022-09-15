// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#include <windows.h>

#include "base/command_line.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_switches.h"
#include "ui/color/color_transform.h"
#include "ui/color/win/accent_color_observer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace ui {

void AddNativeCoreColorMixer(ColorProvider* provider,
                             const ColorProviderManager::Key& key) {
  ColorMixer& mixer = provider->AddMixer();

  // TODO(pkasting): Not clear whether this is really the set of interest.
  // Maybe there's some way to query colors used by UxTheme.dll, or maybe we
  // should be hardcoding a list of colors for system light/dark modes based on
  // reverse-engineering current Windows behavior.  Or maybe the union of all
  // these.
  mixer[kColorNative3dDkShadow] = {
      color_utils::GetSysSkColor(COLOR_3DDKSHADOW)};
  mixer[kColorNative3dLight] = {color_utils::GetSysSkColor(COLOR_3DLIGHT)};
  mixer[kColorNativeActiveBorder] = {
      color_utils::GetSysSkColor(COLOR_ACTIVEBORDER)};
  mixer[kColorNativeActiveCaption] = {
      color_utils::GetSysSkColor(COLOR_ACTIVECAPTION)};
  mixer[kColorNativeAppWorkspace] = {
      color_utils::GetSysSkColor(COLOR_APPWORKSPACE)};
  mixer[kColorNativeBackground] = {
      color_utils::GetSysSkColor(COLOR_BACKGROUND)};
  mixer[kColorNativeBtnFace] = {color_utils::GetSysSkColor(COLOR_BTNFACE)};
  mixer[kColorNativeBtnHighlight] = {
      color_utils::GetSysSkColor(COLOR_BTNHIGHLIGHT)};
  mixer[kColorNativeBtnShadow] = {color_utils::GetSysSkColor(COLOR_BTNSHADOW)};
  mixer[kColorNativeBtnText] = {color_utils::GetSysSkColor(COLOR_BTNTEXT)};
  mixer[kColorNativeCaptionText] = {
      color_utils::GetSysSkColor(COLOR_CAPTIONTEXT)};
  mixer[kColorNativeGradientActiveCaption] = {
      color_utils::GetSysSkColor(COLOR_GRADIENTACTIVECAPTION)};
  mixer[kColorNativeGradientInactiveCaption] = {
      color_utils::GetSysSkColor(COLOR_GRADIENTINACTIVECAPTION)};
  mixer[kColorNativeGrayText] = {color_utils::GetSysSkColor(COLOR_GRAYTEXT)};
  mixer[kColorNativeHighlight] = {color_utils::GetSysSkColor(COLOR_HIGHLIGHT)};
  mixer[kColorNativeHighlightText] = {
      color_utils::GetSysSkColor(COLOR_HIGHLIGHTTEXT)};
  mixer[kColorNativeHotlight] = {color_utils::GetSysSkColor(COLOR_HOTLIGHT)};
  mixer[kColorNativeInactiveBorder] = {
      color_utils::GetSysSkColor(COLOR_INACTIVEBORDER)};
  mixer[kColorNativeInactiveCaption] = {
      color_utils::GetSysSkColor(COLOR_INACTIVECAPTION)};
  mixer[kColorNativeInactiveCaptionText] = {
      color_utils::GetSysSkColor(COLOR_INACTIVECAPTIONTEXT)};
  mixer[kColorNativeInfoBk] = {color_utils::GetSysSkColor(COLOR_INFOBK)};
  mixer[kColorNativeInfoText] = {color_utils::GetSysSkColor(COLOR_INFOTEXT)};
  mixer[kColorNativeMenu] = {color_utils::GetSysSkColor(COLOR_MENU)};
  mixer[kColorNativeMenuBar] = {color_utils::GetSysSkColor(COLOR_MENUBAR)};
  mixer[kColorNativeMenuHilight] = {
      color_utils::GetSysSkColor(COLOR_MENUHILIGHT)};
  mixer[kColorNativeMenuText] = {color_utils::GetSysSkColor(COLOR_MENUTEXT)};
  mixer[kColorNativeScrollbar] = {color_utils::GetSysSkColor(COLOR_SCROLLBAR)};
  mixer[kColorNativeWindow] = {color_utils::GetSysSkColor(COLOR_WINDOW)};
  mixer[kColorNativeWindowFrame] = {
      color_utils::GetSysSkColor(COLOR_WINDOWFRAME)};
  mixer[kColorNativeWindowText] = {
      color_utils::GetSysSkColor(COLOR_WINDOWTEXT)};

  // Use the system accent color as the Chrome accent color, if desired.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPervasiveSystemAccentColor)) {
    const auto accent_color = AccentColorObserver::Get()->accent_color();
    if (accent_color.has_value()) {
      mixer[kColorAccent] =
          PickGoogleColor({accent_color.value()}, kColorPrimaryBackground);
    }
  }

  if (key.contrast_mode == ColorProviderManager::ContrastMode::kNormal)
    return;

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
                           const ColorProviderManager::Key& key) {
  if (key.contrast_mode == ColorProviderManager::ContrastMode::kNormal)
    return;

  ColorMixer& mixer = provider->AddMixer();

  mixer[kColorButtonForegroundChecked] = {
      key.color_mode == ColorProviderManager::ColorMode::kDark
          ? gfx::kGoogleBlue100
          : gfx::kGoogleBlue900};
  mixer[kColorNotificationInputPlaceholderForeground] =
      SetAlpha(kColorNotificationInputForeground, gfx::kGoogleGreyAlpha700);
  mixer[kColorSliderTrack] = AlphaBlend(
      kColorNativeHighlight, kColorNativeWindow, gfx::kGoogleGreyAlpha400);

  // Window Background
  mixer[kColorBubbleFooterBackground] = {kColorNativeWindow};
  mixer[kColorButtonBackgroundProminentDisabled] = {kColorNativeWindow};
  mixer[kColorFrameActive] = {ui::kColorNativeWindow};
  mixer[kColorFrameInactive] = {ui::kColorNativeWindow};
  mixer[kColorTooltipBackground] = {kColorNativeWindow};

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
