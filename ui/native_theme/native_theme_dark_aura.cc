// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_dark_aura.h"

#include "ui/gfx/color_palette.h"

namespace ui {

NativeThemeDarkAura* NativeThemeDarkAura::instance() {
  static base::NoDestructor<NativeThemeDarkAura> s_native_theme;
  return s_native_theme.get();
}

SkColor NativeThemeDarkAura::GetSystemColor(ColorId color_id) const {
  constexpr SkColor kTextSelectionBackgroundFocused =
      SkColorSetA(gfx::kGoogleBlue700, 0xCC);

  switch (color_id) {
    // Window
    case kColorId_WindowBackground:
    case kColorId_DialogBackground:
    case kColorId_BubbleBackground:
      return SK_ColorBLACK;

    // Button
    case kColorId_ButtonEnabledColor:
      return SK_ColorWHITE;
    case kColorId_ProminentButtonColor:
      return gfx::kGoogleBlue600;

    // Label
    case kColorId_LabelEnabledColor:
    case kColorId_LabelTextSelectionColor:
      return SK_ColorWHITE;
    case kColorId_LabelTextSelectionBackgroundFocused:
      return kTextSelectionBackgroundFocused;

    // Link
    case kColorId_LinkEnabled:
    case kColorId_LinkPressed:
      return gfx::kGoogleBlue300;

    // Textfield
    case kColorId_TextfieldDefaultColor:
    case kColorId_TextfieldSelectionColor:
      return SK_ColorWHITE;
    case kColorId_TextfieldDefaultBackground:
      return SkColorSetRGB(0x62, 0x62, 0x62);
    case kColorId_TextfieldSelectionBackgroundFocused:
      return kTextSelectionBackgroundFocused;

    // Results Tables
    case kColorId_ResultsTableNormalBackground:
      return SkColorSetRGB(0x28, 0x28, 0x28);
    case kColorId_ResultsTableNormalText:
      return SK_ColorWHITE;
    case kColorId_ResultsTableDimmedText:
      return SkColorSetA(GetSystemColor(kColorId_ResultsTableNormalText), 0x80);

    // FocusableBorder
    case kColorId_FocusedBorderColor:
      return gfx::kGoogleBlue300;

    // Alert icons
    case kColorId_AlertSeverityLow:
      return gfx::kGoogleGreen300;
    case kColorId_AlertSeverityMedium:
      return gfx::kGoogleYellow300;
    case kColorId_AlertSeverityHigh:
      return gfx::kGoogleRed300;

    // Intentional pass-throughs to NativeThemeAura.
    case kColorId_LabelDisabledColor:
    case kColorId_TextOnProminentButtonColor:
    case kColorId_ButtonPressedShade:
    case kColorId_ResultsTableHoveredBackground:
      return NativeThemeAura::GetSystemColor(color_id);

    // Any other color is not defined and shouldn't be used in a dark theme.
    case kColorId_UnfocusedBorderColor:
    case kColorId_ButtonDisabledColor:
    case kColorId_ButtonHoverColor:
    case kColorId_EnabledMenuItemForegroundColor:
    case kColorId_DisabledMenuItemForegroundColor:
    case kColorId_SelectedMenuItemForegroundColor:
    case kColorId_FocusedMenuItemBackgroundColor:
    case kColorId_TouchableMenuItemLabelColor:
    case kColorId_ActionableSubmenuVerticalSeparatorColor:
    case kColorId_MenuItemMinorTextColor:
    case kColorId_MenuSeparatorColor:
    case kColorId_MenuBackgroundColor:
    case kColorId_MenuBorderColor:
    case kColorId_LinkDisabled:
    case kColorId_TabBottomBorder:
    case kColorId_TabTitleColorActive:
    case kColorId_TabTitleColorInactive:
    case kColorId_TextfieldReadOnlyColor:
    case kColorId_TextfieldReadOnlyBackground:
    case kColorId_TooltipBackground:
    case kColorId_TooltipText:
    case kColorId_TreeBackground:
    case kColorId_TreeText:
    case kColorId_TreeSelectedText:
    case kColorId_TreeSelectedTextUnfocused:
    case kColorId_TreeSelectionBackgroundFocused:
    case kColorId_TreeSelectionBackgroundUnfocused:
    case kColorId_TableBackground:
    case kColorId_TableText:
    case kColorId_TableSelectedText:
    case kColorId_TableSelectedTextUnfocused:
    case kColorId_TableSelectionBackgroundFocused:
    case kColorId_TableSelectionBackgroundUnfocused:
    case kColorId_TableGroupingIndicatorColor:
    case kColorId_TableHeaderText:
    case kColorId_TableHeaderBackground:
    case kColorId_TableHeaderSeparator:
    case kColorId_SeparatorColor:
    case kColorId_ThrobberSpinningColor:
    case kColorId_ThrobberWaitingColor:
    case kColorId_ThrobberLightColor:
    case kColorId_NumColors:
      return gfx::kPlaceholderColor;
  }

  NOTREACHED();
  return gfx::kPlaceholderColor;
}

NativeThemeDarkAura::NativeThemeDarkAura() : NativeThemeAura(false) {}

NativeThemeDarkAura::~NativeThemeDarkAura() {}

}  // namespace ui
