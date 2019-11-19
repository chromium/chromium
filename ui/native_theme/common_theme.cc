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
                     const NativeTheme* base_theme,
                     NativeTheme::ColorScheme color_scheme) {
  if (color_scheme == NativeTheme::ColorScheme::kDefault)
    color_scheme = base_theme->GetDefaultSystemColorScheme();

  // High contrast overrides the normal colors for certain ColorIds to be much
  // darker or lighter.
  if (base_theme->UsesHighContrastColors()) {
    switch (color_id) {
      case NativeTheme::kColorId_ButtonEnabledColor:
      case NativeTheme::kColorId_MenuBorderColor:
      case NativeTheme::kColorId_MenuSeparatorColor:
      case NativeTheme::kColorId_SeparatorColor:
      case NativeTheme::kColorId_UnfocusedBorderColor:
      case NativeTheme::kColorId_TabBottomBorder:
        return color_scheme == NativeTheme::ColorScheme::kDark ? SK_ColorWHITE
                                                               : SK_ColorBLACK;
      case NativeTheme::kColorId_FocusedBorderColor:
      case NativeTheme::kColorId_ProminentButtonColor:
        return color_scheme == NativeTheme::ColorScheme::kDark
                   ? gfx::kGoogleBlue100
                   : gfx::kGoogleBlue900;
      default:
        break;
    }
  }

  if (color_scheme == NativeTheme::ColorScheme::kDark) {
    switch (color_id) {
      // Dialogs
      case NativeTheme::kColorId_WindowBackground:
      case NativeTheme::kColorId_DialogBackground:
      case NativeTheme::kColorId_BubbleBackground:
        return color_utils::AlphaBlend(SK_ColorWHITE, gfx::kGoogleGrey900,
                                       0.04f);
      case NativeTheme::kColorId_DialogForeground:
        return gfx::kGoogleGrey500;
      case NativeTheme::kColorId_BubbleFooterBackground:
        return SkColorSetRGB(0x32, 0x36, 0x39);

      // FocusableBorder
      case NativeTheme::kColorId_FocusedBorderColor:
        return SkColorSetA(gfx::kGoogleBlue300, 0x4D);

      // Button
      case NativeTheme::kColorId_ButtonEnabledColor:
      case NativeTheme::kColorId_ProminentButtonColor:
        return gfx::kGoogleBlue300;
      case NativeTheme::kColorId_ProminentButtonDisabledColor:
        return gfx::kGoogleGrey800;
      case NativeTheme::kColorId_TextOnProminentButtonColor:
        return gfx::kGoogleGrey900;
      case NativeTheme::kColorId_ButtonBorderColor:
        return gfx::kGoogleGrey700;

      // MenuItem
      case NativeTheme::kColorId_EnabledMenuItemForegroundColor:
      case NativeTheme::kColorId_SelectedMenuItemForegroundColor:
      case NativeTheme::kColorId_HighlightedMenuItemForegroundColor:
        return gfx::kGoogleGrey200;
      case NativeTheme::kColorId_MenuSeparatorColor:
        return gfx::kGoogleGrey800;
      case NativeTheme::kColorId_MenuBackgroundColor:
        return color_utils::AlphaBlend(SK_ColorWHITE, gfx::kGoogleGrey900,
                                       0.04f);
      case NativeTheme::kColorId_HighlightedMenuItemBackgroundColor:
        return SkColorSetRGB(0x32, 0x36, 0x39);
      case NativeTheme::kColorId_MenuItemAlertBackgroundColor:
        return gfx::kGoogleBlue300;

      // Label
      case NativeTheme::kColorId_LabelEnabledColor:
        return gfx::kGoogleGrey200;
      case NativeTheme::kColorId_LabelTextSelectionColor:
        return color_utils::AlphaBlend(
            SK_ColorWHITE,
            GetAuraColor(
                NativeTheme::kColorId_LabelTextSelectionBackgroundFocused,
                base_theme, color_scheme),
            SkAlpha{0xDD});
      case NativeTheme::kColorId_LabelTextSelectionBackgroundFocused:
        return SkColorSetA(gfx::kGoogleBlue700, 0xCC);

      // Link
      case NativeTheme::kColorId_LinkEnabled:
      case NativeTheme::kColorId_LinkPressed:
        return gfx::kGoogleBlue300;

      // Separator
      case NativeTheme::kColorId_SeparatorColor:
        return SkColorSetA(gfx::kGoogleGrey800, 0xCC);

      // Table
      case NativeTheme::kColorId_TableBackground:
        return color_utils::AlphaBlend(SK_ColorWHITE, gfx::kGoogleGrey900,
                                       0.04f);
      case NativeTheme::kColorId_TableText:
      case NativeTheme::kColorId_TableSelectedText:
      case NativeTheme::kColorId_TableSelectedTextUnfocused:
        return gfx::kGoogleGrey200;

      // Textfield
      case NativeTheme::kColorId_TextfieldDefaultColor:
        return gfx::kGoogleGrey200;
      case NativeTheme::kColorId_TextfieldDefaultBackground:
        return SkColorSetA(SK_ColorBLACK, 0x4D);
      case NativeTheme::kColorId_TextfieldSelectionColor:
        return color_utils::AlphaBlend(
            SK_ColorWHITE,
            GetAuraColor(
                NativeTheme::kColorId_LabelTextSelectionBackgroundFocused,
                base_theme, color_scheme),
            SkAlpha{0xDD});
      case NativeTheme::kColorId_TextfieldSelectionBackgroundFocused:
        return SkColorSetA(gfx::kGoogleBlue700, 0xCC);

      // Tree
      case NativeTheme::kColorId_TreeBackground:
        return color_utils::AlphaBlend(SK_ColorWHITE, gfx::kGoogleGrey900,
                                       0.04f);
      case NativeTheme::kColorId_TreeText:
      case NativeTheme::kColorId_TreeSelectedText:
      case NativeTheme::kColorId_TreeSelectedTextUnfocused:
        return gfx::kGoogleGrey200;

      // Material spinner/throbber
      case NativeTheme::kColorId_ThrobberSpinningColor:
        return gfx::kGoogleBlue300;

      // Alert icon colors
      case NativeTheme::kColorId_AlertSeverityLow:
        return gfx::kGoogleGreen300;
      case NativeTheme::kColorId_AlertSeverityMedium:
        return gfx::kGoogleYellow300;
      case NativeTheme::kColorId_AlertSeverityHigh:
        return gfx::kGoogleRed300;

      case NativeTheme::kColorId_DefaultIconColor:
        return gfx::kGoogleGrey500;
      default:
        break;
    }
  }

  constexpr SkColor kPrimaryTextColor = gfx::kGoogleGrey900;

  // Text selection colors:
  constexpr SkColor kTextSelectionBackgroundFocused =
      SkColorSetARGB(0x54, 0x60, 0xA8, 0xEB);
  static const SkColor kTextSelectionColor = color_utils::AlphaBlend(
      SK_ColorBLACK, kTextSelectionBackgroundFocused, SkAlpha{0xDD});

  switch (color_id) {
    // Dialogs
    case NativeTheme::kColorId_WindowBackground:
    case NativeTheme::kColorId_DialogBackground:
    case NativeTheme::kColorId_BubbleBackground:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_DialogForeground:
      return gfx::kGoogleGrey700;
    case NativeTheme::kColorId_BubbleFooterBackground:
      return gfx::kGoogleGrey050;

    // Buttons
    case NativeTheme::kColorId_ButtonEnabledColor:
      return gfx::kGoogleBlue600;
    case NativeTheme::kColorId_ProminentButtonFocusedColor:
      return gfx::kGoogleBlue400;
    case NativeTheme::kColorId_ProminentButtonColor:
      return gfx::kGoogleBlue500;
    case NativeTheme::kColorId_TextOnProminentButtonColor:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_ButtonPressedShade:
      return SK_ColorTRANSPARENT;
    case NativeTheme::kColorId_ButtonDisabledColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_DialogBackground, color_scheme);
      const SkColor fg = base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
      return color_utils::BlendForMinContrast(gfx::kGoogleGrey600, bg, fg)
          .color;
    }
    case NativeTheme::kColorId_ProminentButtonDisabledColor:
      return gfx::kGoogleGrey100;
    case NativeTheme::kColorId_ButtonBorderColor:
      return gfx::kGoogleGrey300;

    // MenuItem
    case NativeTheme::kColorId_EnabledMenuItemForegroundColor:
    case NativeTheme::kColorId_SelectedMenuItemForegroundColor:
    case NativeTheme::kColorId_HighlightedMenuItemForegroundColor:
      return kPrimaryTextColor;
    case NativeTheme::kColorId_FocusedMenuItemBackgroundColor:
    case NativeTheme::kColorId_MenuBorderColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_MenuBackgroundColor, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.67f)
          .color;
    }
    case NativeTheme::kColorId_MenuSeparatorColor:
      return gfx::kGoogleGrey300;
    case NativeTheme::kColorId_MenuBackgroundColor:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_DisabledMenuItemForegroundColor:
    case NativeTheme::kColorId_MenuItemMinorTextColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_MenuBackgroundColor, color_scheme);
      const SkColor fg = base_theme->GetSystemColor(
          NativeTheme::kColorId_EnabledMenuItemForegroundColor, color_scheme);
      return color_utils::BlendForMinContrast(gfx::kGoogleGrey600, bg, fg)
          .color;
    }
    case NativeTheme::kColorId_HighlightedMenuItemBackgroundColor:
      return gfx::kGoogleGrey050;
    case NativeTheme::kColorId_MenuItemAlertBackgroundColor:
      return gfx::kGoogleBlue600;

    // Label
    case NativeTheme::kColorId_LabelEnabledColor:
      return kPrimaryTextColor;
    case NativeTheme::kColorId_LabelDisabledColor:
      return SkColorSetA(
          base_theme->GetSystemColor(NativeTheme::kColorId_LabelEnabledColor,
                                     color_scheme),
          gfx::kDisabledControlAlpha);
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
      return kPrimaryTextColor;
    case NativeTheme::kColorId_TextfieldDefaultBackground:
      return base_theme->GetSystemColor(NativeTheme::kColorId_DialogBackground,
                                        color_scheme);
    case NativeTheme::kColorId_TextfieldReadOnlyBackground:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_TextfieldReadOnlyColor:
      return SkColorSetA(
          base_theme->GetSystemColor(
              NativeTheme::kColorId_TextfieldDefaultColor, color_scheme),
          gfx::kDisabledControlAlpha);

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
      return kPrimaryTextColor;
    case NativeTheme::kColorId_TreeSelectionBackgroundFocused:
    case NativeTheme::kColorId_TreeSelectionBackgroundUnfocused: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_TreeBackground, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.67f)
          .color;
    }

    // Table
    case NativeTheme::kColorId_TableBackground:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_TableText:
    case NativeTheme::kColorId_TableSelectedText:
    case NativeTheme::kColorId_TableSelectedTextUnfocused:
      return kPrimaryTextColor;
    case NativeTheme::kColorId_TableSelectionBackgroundFocused:
    case NativeTheme::kColorId_TableSelectionBackgroundUnfocused:
    case NativeTheme::kColorId_TableGroupingIndicatorColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_TableBackground, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.67f)
          .color;
    }

    // Table Header
    case NativeTheme::kColorId_TableHeaderText:
      return base_theme->GetSystemColor(NativeTheme::kColorId_TableText,
                                        color_scheme);
    case NativeTheme::kColorId_TableHeaderBackground:
      return base_theme->GetSystemColor(NativeTheme::kColorId_TableBackground,
                                        color_scheme);
    case NativeTheme::kColorId_TableHeaderSeparator:
      return base_theme->GetSystemColor(NativeTheme::kColorId_MenuBorderColor,
                                        color_scheme);

    // FocusableBorder
    case NativeTheme::kColorId_FocusedBorderColor:
      return SkColorSetA(gfx::kGoogleBlue600, 0x4D);
    case NativeTheme::kColorId_UnfocusedBorderColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.67f)
          .color;
    }

    // Material spinner/throbber
    case NativeTheme::kColorId_ThrobberSpinningColor:
      return gfx::kGoogleBlue600;
    case NativeTheme::kColorId_ThrobberWaitingColor:
      return SkColorSetRGB(0xA6, 0xA6, 0xA6);
    case NativeTheme::kColorId_ThrobberLightColor:
      return SkColorSetRGB(0xF4, 0xF8, 0xFD);

    // Alert icon colors
    case NativeTheme::kColorId_AlertSeverityLow:
      return gfx::kGoogleGreen700;
    case NativeTheme::kColorId_AlertSeverityMedium:
      return gfx::kGoogleYellow700;
    case NativeTheme::kColorId_AlertSeverityHigh:
      return gfx::kGoogleRed600;

    case NativeTheme::kColorId_DefaultIconColor:
      return gfx::kGoogleGrey700;

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
    const NativeTheme::MenuItemExtraParams& menu_item,
    NativeTheme::ColorScheme color_scheme) {
  cc::PaintFlags flags;
  switch (state) {
    case NativeTheme::kNormal:
    case NativeTheme::kDisabled:
      flags.setColor(theme->GetSystemColor(
          NativeTheme::kColorId_MenuBackgroundColor, color_scheme));
      break;
    case NativeTheme::kHovered:
      flags.setColor(theme->GetSystemColor(
          NativeTheme::kColorId_FocusedMenuItemBackgroundColor, color_scheme));
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
