// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/command_line.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/color/win/accent_color_observer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace ui {

// Maps the base set of sys color ids to appropriate platform defined high
// contrast colors.
void AddHighContrastSysColors(ColorMixer& mixer) {
  // Primary.
  mixer[kColorSysPrimary] = {kColorNativeWindow};
  mixer[kColorSysOnPrimary] = {kColorNativeWindowText};
  mixer[kColorSysPrimaryContainer] = {kColorNativeBtnFace};
  mixer[kColorSysOnPrimaryContainer] = {kColorNativeBtnText};
  // Secondary.
  mixer[kColorSysSecondary] = {kColorNativeWindow};
  mixer[kColorSysOnSecondary] = {kColorNativeWindowText};
  mixer[kColorSysSecondaryContainer] = {kColorNativeBtnFace};
  mixer[kColorSysOnSecondaryContainer] = {kColorNativeBtnText};
  // Tertiary.
  mixer[kColorSysTertiary] = {kColorNativeWindow};
  mixer[kColorSysOnTertiary] = {kColorNativeWindowText};
  mixer[kColorSysTertiaryContainer] = {kColorNativeBtnFace};
  mixer[kColorSysOnTertiaryContainer] = {kColorNativeBtnText};
  // Error.
  mixer[kColorSysError] = {kColorNativeWindow};
  mixer[kColorSysOnError] = {kColorNativeWindowText};
  mixer[kColorSysErrorContainer] = {kColorNativeBtnFace};
  mixer[kColorSysOnErrorContainer] = {kColorNativeBtnText};
  // Neutral.
  mixer[kColorSysOnSurface] = {kColorNativeBtnText};
  mixer[kColorSysOnSurfaceVariant] = {kColorNativeBtnText};
  mixer[kColorSysOutline] = {kColorNativeBtnText};
  mixer[kColorSysSurfaceVariant] = {kColorNativeBtnFace};
  // Inverse.
  mixer[kColorSysInversePrimary] = {kColorNativeWindow};
  mixer[kColorSysInverseSurface] = {kColorNativeWindow};
  mixer[kColorSysInverseOnSurface] = {kColorNativeWindowText};
  // Surfaces.
  mixer[kColorSysSurface] = {kColorNativeWindow};
  mixer[kColorSysSurface1] = {kColorNativeWindow};
  mixer[kColorSysSurface2] = {kColorNativeWindow};
  mixer[kColorSysSurface3] = {kColorNativeWindow};
  mixer[kColorSysSurface4] = {kColorNativeWindow};
  mixer[kColorSysSurface5] = {kColorNativeWindow};
  // General.
  mixer[kColorSysOnSurfaceSecondary] = {kColorNativeWindowText};
  mixer[kColorSysOnSurfaceSubtle] = {kColorNativeWindowText};
  mixer[kColorSysOnSurfacePrimary] = {kColorNativeWindowText};
  mixer[kColorSysOnSurfacePrimaryInactive] = {kColorNativeWindowText};
  mixer[kColorSysTonalContainer] = {kColorNativeBtnFace};
  mixer[kColorSysOnTonalContainer] = {kColorNativeBtnText};
  mixer[kColorSysTonalOutline] = {kColorNativeBtnText};
  mixer[kColorSysNeutralOutline] = {kColorNativeBtnText};
  mixer[kColorSysNeutralContainer] = {kColorNativeBtnFace};
  mixer[kColorSysDivider] = {kColorNativeBtnText};
  // Chrome surfaces.
  mixer[kColorSysBase] = {kColorNativeBtnFace};
  mixer[kColorSysBaseContainer] = {kColorNativeBtnFace};
  mixer[kColorSysBaseContainerElevated] = {kColorNativeBtnFace};
  mixer[kColorSysHeader] = {kColorNativeWindow};
  mixer[kColorSysHeaderInactive] = {kColorNativeWindow};
  mixer[kColorSysHeaderContainer] = {kColorNativeBtnFace};
  mixer[kColorSysHeaderContainerInactive] = {kColorNativeBtnFace};
  mixer[kColorSysOnHeaderDivider] = {kColorNativeWindowText};
  mixer[kColorSysOnHeaderDividerInactive] = {kColorNativeWindowText};
  mixer[kColorSysOnHeaderPrimary] = {kColorNativeWindowText};
  mixer[kColorSysOnHeaderPrimaryInactive] = {kColorNativeWindowText};
  // States.
  mixer[kColorSysStateDisabled] = {kColorNativeGrayText};
  mixer[kColorSysStateDisabledContainer] = {kColorNativeWindow};
  // Effects.
  mixer[kColorSysShadow] = {kColorNativeBtnShadow};
  // Experimentation.
  mixer[kColorSysOmniboxContainer] = {kColorNativeBtnFace};
}

void AddNativeCoreColorMixer(ColorProvider* provider,
                             const ColorProviderKey& key) {
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

  // Use the system accent color as the Chrome accent color, if present and only
  // if dwm colors are enabled.
  const auto* accent_color_observer = AccentColorObserver::Get();
  const auto& accent_color = accent_color_observer->accent_color();
  if (accent_color.has_value() &&
      accent_color_observer->use_dwm_frame_color()) {
    mixer[kColorAccent] = PickGoogleColor(accent_color.value());
  }

  if (key.contrast_mode == ColorProviderKey::ContrastMode::kHigh) {
    AddHighContrastSysColors(mixer);
  }
}

void AddNativeUiColorMixer(ColorProvider* provider,
                           const ColorProviderKey& key) {
  if (key.contrast_mode == ColorProviderKey::ContrastMode::kNormal) {
    return;
  }

  ColorMixer& mixer = provider->AddMixer();

  mixer[kColorNotificationInputPlaceholderForeground] =
      SetAlpha(kColorNotificationInputForeground, gfx::kGoogleGreyAlpha700);
  mixer[kColorSliderTrack] = AlphaBlend(
      kColorNativeHighlight, kColorNativeWindow, gfx::kGoogleGreyAlpha400);

  // Window Background
  mixer[kColorBubbleFooterBackground] = {kColorNativeWindow};
  mixer[kColorButtonBackgroundProminentDisabled] = {kColorNativeWindow};
  mixer[kColorFrameActive] = {kColorNativeWindow};
  mixer[kColorFrameInactive] = {kColorNativeWindow};
  mixer[kColorPrimaryBackground] = {kColorNativeWindow};
  mixer[kColorTooltipBackground] = {kColorNativeWindow};

  // Window Text
  mixer[kColorAlertLowSeverity] = {kColorNativeWindowText};
  mixer[kColorAlertMediumSeverityIcon] = {kColorNativeWindowText};
  mixer[kColorAlertMediumSeverityText] = {kColorNativeWindowText};
  mixer[kColorAlertHighSeverity] = {kColorNativeWindowText};
  mixer[kColorIcon] = {kColorNativeWindowText};
  mixer[kColorMidground] = {kColorNativeWindowText};
  mixer[kColorPrimaryForeground] = {kColorNativeWindowText};
  mixer[kColorSecondaryForeground] = {kColorNativeWindowText};
  mixer[kColorTableGroupingIndicator] = {kColorNativeWindowText};
  mixer[kColorThrobber] = {kColorNativeWindowText};
  mixer[kColorTooltipForeground] = {kColorNativeWindowText};

  // Hyperlinks
  mixer[kColorLinkForegroundDefault] = {kColorNativeHotlight};
  mixer[kColorMenuItemForegroundHighlighted] = {kColorNativeHotlight};

  // Gray/Disabled Text
  mixer[kColorDisabledForeground] = {kColorNativeGrayText};
  mixer[kColorMenuItemForegroundDisabled] = {kColorNativeGrayText};
  mixer[kColorLinkForegroundDisabled] = {kColorNativeGrayText};
  mixer[kColorLabelForegroundDisabled] = {kColorNativeGrayText};
  mixer[kColorButtonForegroundDisabled] = {kColorNativeGrayText};
  mixer[kColorThrobberPreconnect] = {kColorNativeGrayText};

  // Button Background
  mixer[kColorButtonBackground] = {kColorNativeBtnFace};
  mixer[kColorComboboxBackground] = {kColorNativeBtnFace};
  mixer[kColorMenuBackground] = {kColorNativeBtnFace};
  mixer[kColorSliderTrack] = {kColorNativeBtnFace};
  mixer[kColorSliderTrackMinimal] = {kColorNativeBtnFace};
  mixer[kColorSubtleEmphasisBackground] = {kColorNativeBtnFace};
  mixer[kColorTextfieldBackground] = {kColorNativeBtnFace};
  mixer[kColorTextfieldBackgroundDisabled] = {kColorNativeBtnFace};

  // Button Text Foreground
  mixer[kColorButtonForeground] = {kColorNativeBtnText};
  mixer[kColorCheckboxForegroundChecked] = {kColorNativeBtnText};
  mixer[kColorFocusableBorderFocused] = {kColorNativeBtnText};
  mixer[kColorFocusableBorderUnfocused] = {kColorNativeBtnText};
  mixer[kColorMenuBorder] = {kColorNativeBtnText};
  mixer[kColorMenuItemForeground] = {kColorNativeBtnText};
  mixer[kColorMenuItemForegroundSecondary] = {kColorNativeBtnText};
  mixer[kColorMenuSeparator] = {kColorNativeBtnText};
  mixer[kColorRadioButtonForegroundChecked] = {kColorNativeBtnText};
  mixer[kColorSeparator] = {kColorNativeBtnText};
  mixer[kColorSliderThumb] = {kColorNativeBtnText};
  mixer[kColorSliderThumbMinimal] = {kColorNativeBtnText};
  mixer[kColorTabContentSeparator] = {kColorNativeBtnText};
  mixer[kColorTabForeground] = {kColorNativeBtnText};
  mixer[kColorTabForegroundSelected] = {kColorNativeBtnText};
  mixer[kColorTextfieldForeground] = {kColorNativeBtnText};
  mixer[kColorTextfieldForegroundPlaceholder] = {kColorNativeBtnText};
  mixer[kColorTextfieldForegroundDisabled] = {kColorNativeBtnText};

  // Highlight/Selected Background
  mixer[kColorAccent] = {kColorNativeHighlight};
  mixer[kColorButtonBackgroundProminent] = {kColorNativeHighlight};
  mixer[kColorButtonBorder] = {kColorNativeHighlight};
  mixer[kColorButtonBackgroundProminentFocused] = {kColorNativeHighlight};
  mixer[kColorHelpIconActive] = {kColorNativeHighlight};
  mixer[kColorItemSelectionBackground] = {kColorNativeHighlight};
  mixer[kColorMenuSelectionBackground] = {kColorNativeHighlight};
  mixer[kColorSubtleAccent] = {kColorNativeHighlight};
  mixer[kColorTextfieldSelectionBackground] = {kColorNativeHighlight};
  mixer[kColorTextSelectionBackground] = {kColorNativeHighlight};

  // Highlight/Selected Text Foreground
  mixer[kColorButtonForegroundProminent] = {kColorNativeHighlightText};
  mixer[kColorMenuItemForegroundSelected] = {kColorNativeHighlightText};
  mixer[kColorNotificationInputForeground] = {kColorNativeHighlightText};
  mixer[kColorTableForegroundSelectedFocused] = {kColorNativeHighlightText};
  mixer[kColorTableForegroundSelectedUnfocused] = {kColorNativeHighlightText};
  mixer[kColorTextSelectionForeground] = {kColorNativeHighlightText};
  mixer[kColorTreeNodeForegroundSelectedFocused] = {kColorNativeHighlightText};
  mixer[kColorTreeNodeForegroundSelectedUnfocused] = {
      kColorNativeHighlightText};
  mixer[kColorButtonForegroundProminent] = {kColorNativeHighlightText};
}

}  // namespace ui
