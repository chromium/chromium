// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/common_theme.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"

namespace ui {

SkColor GetAuraColor(NativeTheme::ColorId color_id,
                     const NativeTheme* base_theme) {
  // High contrast overrides the normal colors for certain ColorIds to be much
  // darker or lighter.
  if (base_theme->UsesHighContrastColors()) {
    switch (color_id) {
      case NativeTheme::kColorId_ButtonEnabledColor:
      case NativeTheme::kColorId_ButtonHoverColor:
      case NativeTheme::kColorId_MenuBorderColor:
      case NativeTheme::kColorId_MenuSeparatorColor:
      case NativeTheme::kColorId_SeparatorColor:
      case NativeTheme::kColorId_UnfocusedBorderColor:
      case NativeTheme::kColorId_TabBottomBorder:
        return SK_ColorBLACK;
      case NativeTheme::kColorId_FocusedBorderColor:
      case NativeTheme::kColorId_ProminentButtonColor:
        return gfx::kGoogleBlue900;
      default:
        break;
    }
  }

  // Second wave of MD colors (colors that only appear in secondary UI).
  constexpr SkColor kPrimaryTextColor = gfx::kGoogleGrey900;

  switch (color_id) {
    // Labels
    case NativeTheme::kColorId_LabelEnabledColor:
      return kPrimaryTextColor;
    case NativeTheme::kColorId_LabelDisabledColor:
      return SkColorSetA(
          base_theme->GetSystemColor(NativeTheme::kColorId_LabelEnabledColor),
          gfx::kDisabledControlAlpha);

    // FocusableBorder
    case NativeTheme::kColorId_UnfocusedBorderColor:
      return SkColorSetA(SK_ColorBLACK, 0x4e);

    // Textfields
    case NativeTheme::kColorId_TextfieldDefaultColor:
      return kPrimaryTextColor;
    case NativeTheme::kColorId_TextfieldDefaultBackground:
      return base_theme->GetSystemColor(NativeTheme::kColorId_DialogBackground);
    case NativeTheme::kColorId_TextfieldReadOnlyColor:
      return SkColorSetA(base_theme->GetSystemColor(
                             NativeTheme::kColorId_TextfieldDefaultColor),
                         gfx::kDisabledControlAlpha);

    default:
      break;
  }

  // Shared constant for disabled text.
  constexpr SkColor kDisabledTextColor = SkColorSetRGB(0xA1, 0xA1, 0x92);

  // Buttons:
  constexpr SkColor kButtonEnabledColor = gfx::kChromeIconGrey;
  // Text selection colors:
  constexpr SkColor kTextSelectionBackgroundFocused =
      SkColorSetARGB(0x54, 0x60, 0xA8, 0xEB);
  static const SkColor kTextSelectionColor = color_utils::AlphaBlend(
      SK_ColorBLACK, kTextSelectionBackgroundFocused, 0xdd);

  switch (color_id) {
    // Dialogs
    case NativeTheme::kColorId_WindowBackground:
    case NativeTheme::kColorId_DialogBackground:
    case NativeTheme::kColorId_BubbleBackground:
      return SK_ColorWHITE;

    // Buttons
    case NativeTheme::kColorId_ButtonEnabledColor:
    case NativeTheme::kColorId_ButtonHoverColor:
      return kButtonEnabledColor;
    case NativeTheme::kColorId_ProminentButtonColor:
      return gfx::kGoogleBlue500;
    case NativeTheme::kColorId_TextOnProminentButtonColor:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_ButtonPressedShade:
      return SK_ColorTRANSPARENT;
    case NativeTheme::kColorId_ButtonDisabledColor:
      return kDisabledTextColor;

    // MenuItem
    case NativeTheme::kColorId_TouchableMenuItemLabelColor:
      return gfx::kGoogleGrey900;
    case NativeTheme::kColorId_ActionableSubmenuVerticalSeparatorColor:
      return SkColorSetA(gfx::kGoogleGrey900, 0x24);
    case NativeTheme::kColorId_SelectedMenuItemForegroundColor:
    case NativeTheme::kColorId_EnabledMenuItemForegroundColor:
      return SK_ColorBLACK;
    case NativeTheme::kColorId_MenuBorderColor:
      return SkColorSetRGB(0xBA, 0xBA, 0xBA);
    case NativeTheme::kColorId_MenuSeparatorColor:
      return SkColorSetRGB(0xE9, 0xE9, 0xE9);
    case NativeTheme::kColorId_MenuBackgroundColor:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_FocusedMenuItemBackgroundColor:
      return SkColorSetA(SK_ColorBLACK, 0x14);
    case NativeTheme::kColorId_DisabledMenuItemForegroundColor:
      return kDisabledTextColor;
    case NativeTheme::kColorId_MenuItemMinorTextColor:
      return SkColorSetA(SK_ColorBLACK, 0x89);

    // Label
    case NativeTheme::kColorId_LabelEnabledColor:
      return kButtonEnabledColor;
    case NativeTheme::kColorId_LabelDisabledColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_ButtonDisabledColor);
    case NativeTheme::kColorId_LabelTextSelectionColor:
      return kTextSelectionColor;
    case NativeTheme::kColorId_LabelTextSelectionBackgroundFocused:
      return kTextSelectionBackgroundFocused;

    // Link
    // TODO(estade): where, if anywhere, do we use disabled links in Chrome?
    case NativeTheme::kColorId_LinkDisabled:
      return SK_ColorBLACK;

    case NativeTheme::kColorId_LinkEnabled:
    case NativeTheme::kColorId_LinkPressed:
      return gfx::kGoogleBlue700;

    // Separator
    case NativeTheme::kColorId_SeparatorColor:
      return SkColorSetRGB(0xE9, 0xE9, 0xE9);

    // TabbedPane
    case NativeTheme::kColorId_TabTitleColorActive:
      return SkColorSetRGB(0x42, 0x85, 0xF4);
    case NativeTheme::kColorId_TabTitleColorInactive:
      return SkColorSetRGB(0x75, 0x75, 0x75);
    case NativeTheme::kColorId_TabBottomBorder:
      return SkColorSetA(SK_ColorBLACK, 0x1E);

    // Textfield
    case NativeTheme::kColorId_TextfieldDefaultColor:
      return SK_ColorBLACK;
    case NativeTheme::kColorId_TextfieldDefaultBackground:
    case NativeTheme::kColorId_TextfieldReadOnlyBackground:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_TextfieldReadOnlyColor:
      return kDisabledTextColor;
    case NativeTheme::kColorId_TextfieldSelectionColor:
      return kTextSelectionColor;
    case NativeTheme::kColorId_TextfieldSelectionBackgroundFocused:
      return kTextSelectionBackgroundFocused;

    // Tooltip
    case NativeTheme::kColorId_TooltipBackground:
      return SkColorSetA(SK_ColorBLACK, 0xCC);
    case NativeTheme::kColorId_TooltipText:
      return SkColorSetA(SK_ColorWHITE, 0xDE);

    // Tree
    case NativeTheme::kColorId_TreeBackground:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_TreeText:
    case NativeTheme::kColorId_TreeSelectedText:
    case NativeTheme::kColorId_TreeSelectedTextUnfocused:
      return SK_ColorBLACK;
    case NativeTheme::kColorId_TreeSelectionBackgroundFocused:
    case NativeTheme::kColorId_TreeSelectionBackgroundUnfocused:
      return SkColorSetRGB(0xEE, 0xEE, 0xEE);

    // Table
    case NativeTheme::kColorId_TableBackground:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_TableText:
    case NativeTheme::kColorId_TableSelectedText:
    case NativeTheme::kColorId_TableSelectedTextUnfocused:
      return SK_ColorBLACK;
    case NativeTheme::kColorId_TableSelectionBackgroundFocused:
    case NativeTheme::kColorId_TableSelectionBackgroundUnfocused:
      return SkColorSetRGB(0xEE, 0xEE, 0xEE);
    case NativeTheme::kColorId_TableGroupingIndicatorColor:
      return SkColorSetRGB(0xCC, 0xCC, 0xCC);

    // Table Header
    case NativeTheme::kColorId_TableHeaderText:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_EnabledMenuItemForegroundColor);
    case NativeTheme::kColorId_TableHeaderBackground:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_MenuBackgroundColor);
    case NativeTheme::kColorId_TableHeaderSeparator:
      return base_theme->GetSystemColor(NativeTheme::kColorId_MenuBorderColor);

    // FocusableBorder
    case NativeTheme::kColorId_FocusedBorderColor:
      return gfx::kGoogleBlue500;
    case NativeTheme::kColorId_UnfocusedBorderColor:
      return SkColorSetA(SK_ColorBLACK, 0x66);

    // Results Tables
    case NativeTheme::kColorId_ResultsTableNormalBackground:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_ResultsTableHoveredBackground:
      return SkColorSetA(base_theme->GetSystemColor(
                             NativeTheme::kColorId_ResultsTableNormalText),
                         0x0D);
    case NativeTheme::kColorId_ResultsTableNormalText:
      return SK_ColorBLACK;
    case NativeTheme::kColorId_ResultsTableDimmedText:
      return SkColorSetRGB(0x64, 0x64, 0x64);

    // Material spinner/throbber
    case NativeTheme::kColorId_ThrobberSpinningColor:
      return gfx::kGoogleBlue600;
    case NativeTheme::kColorId_ThrobberWaitingColor:
      return SkColorSetRGB(0xA6, 0xA6, 0xA6);
    case NativeTheme::kColorId_ThrobberLightColor:
      return SkColorSetRGB(0xF4, 0xF8, 0xFD);

    // Alert icon colors
    case NativeTheme::kColorId_AlertSeverityLow:
      return gfx::kGoogleGreen600;
    case NativeTheme::kColorId_AlertSeverityMedium:
      return gfx::kGoogleYellow700;
    case NativeTheme::kColorId_AlertSeverityHigh:
      return gfx::kGoogleRed600;

    case NativeTheme::kColorId_NumColors:
      break;
  }

  NOTREACHED();
  return gfx::kPlaceholderColor;
}

void CommonThemePaintMenuItemBackground(
    const NativeTheme* theme,
    cc::PaintCanvas* canvas,
    NativeTheme::State state,
    const gfx::Rect& rect,
    const NativeTheme::MenuItemExtraParams& menu_item) {
  cc::PaintFlags flags;
  switch (state) {
    case NativeTheme::kNormal:
    case NativeTheme::kDisabled:
      flags.setColor(
          theme->GetSystemColor(NativeTheme::kColorId_MenuBackgroundColor));
      break;
    case NativeTheme::kHovered:
      flags.setColor(theme->GetSystemColor(
          NativeTheme::kColorId_FocusedMenuItemBackgroundColor));
      break;
    default:
      NOTREACHED() << "Invalid state " << state;
      break;
  }
  if (menu_item.corner_radius > 0) {
    const SkScalar radius = SkIntToScalar(menu_item.corner_radius);
    canvas->drawRoundRect(gfx::RectToSkRect(rect), radius, radius, flags);
    return;
  }
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

}  // namespace ui
