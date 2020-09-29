// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/common_theme.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/overlay_scrollbar_constants_aura.h"

namespace ui {

namespace {

NativeTheme::SecurityChipColorId GetSecurityChipColorId(
    NativeTheme::ColorId color_id) {
  static const base::NoDestructor<
      base::flat_map<NativeTheme::ColorId, NativeTheme::SecurityChipColorId>>
      color_id_map({
          {NativeTheme::kColorId_CustomTabBarSecurityChipDefaultColor,
           NativeTheme::SecurityChipColorId::DEFAULT},
          {NativeTheme::kColorId_CustomTabBarSecurityChipSecureColor,
           NativeTheme::SecurityChipColorId::SECURE},
          {NativeTheme::kColorId_CustomTabBarSecurityChipWithCertColor,
           NativeTheme::SecurityChipColorId::SECURE_WITH_CERT},
          {NativeTheme::kColorId_CustomTabBarSecurityChipDangerousColor,
           NativeTheme::SecurityChipColorId::DANGEROUS},
      });
  return color_id_map->at(color_id);
}

base::Optional<SkColor> GetHighContrastColor(
    NativeTheme::ColorId color_id,
    NativeTheme::ColorScheme color_scheme) {
  switch (color_id) {
    case NativeTheme::kColorId_ButtonUncheckedColor:
    case NativeTheme::kColorId_MenuBorderColor:
    case NativeTheme::kColorId_MenuSeparatorColor:
    case NativeTheme::kColorId_SeparatorColor:
    case NativeTheme::kColorId_UnfocusedBorderColor:
    case NativeTheme::kColorId_TabBottomBorder:
      return color_scheme == NativeTheme::ColorScheme::kDark ? SK_ColorWHITE
                                                             : SK_ColorBLACK;
    case NativeTheme::kColorId_ButtonCheckedColor:
    case NativeTheme::kColorId_ButtonEnabledColor:
    case NativeTheme::kColorId_FocusedBorderColor:
    case NativeTheme::kColorId_ProminentButtonColor:
      return color_scheme == NativeTheme::ColorScheme::kDark
                 ? gfx::kGoogleBlue100
                 : gfx::kGoogleBlue900;
    default:
      return base::nullopt;
  }
}

base::Optional<SkColor> GetDarkSchemeColor(NativeTheme::ColorId color_id) {
  switch (color_id) {
    // Dialogs
    case NativeTheme::kColorId_WindowBackground:
    case NativeTheme::kColorId_ButtonColor:
    case NativeTheme::kColorId_DialogBackground:
    case NativeTheme::kColorId_BubbleBackground:
    case NativeTheme::kColorId_NotificationDefaultBackground:
      return color_utils::AlphaBlend(SK_ColorWHITE, gfx::kGoogleGrey900, 0.04f);
    case NativeTheme::kColorId_DialogForeground:
      return gfx::kGoogleGrey500;
    case NativeTheme::kColorId_BubbleForeground:
      return gfx::kGoogleGrey200;
    case NativeTheme::kColorId_BubbleFooterBackground:
      return SkColorSetRGB(0x32, 0x36, 0x39);

    // FocusableBorder
    case NativeTheme::kColorId_FocusedBorderColor:
      return SkColorSetA(gfx::kGoogleBlue300, 0x4D);
    case NativeTheme::kColorId_UnfocusedBorderColor:
      return gfx::kGoogleGrey800;

    // Button
    case NativeTheme::kColorId_ButtonBorderColor:
      return gfx::kGoogleGrey800;
    case NativeTheme::kColorId_ButtonCheckedColor:
    case NativeTheme::kColorId_ButtonEnabledColor:
    case NativeTheme::kColorId_ProminentButtonColor:
      return gfx::kGoogleBlue300;
    case NativeTheme::kColorId_ButtonHoverColor:
      return SkColorSetA(SK_ColorBLACK, 0x0A);
    case NativeTheme::kColorId_ButtonInkDropShadowColor:
      return SkColorSetA(SK_ColorBLACK, 0x7F);
    case NativeTheme::kColorId_ButtonInkDropFillColor:
    case NativeTheme::kColorId_ProminentButtonInkDropFillColor:
      return SkColorSetA(SK_ColorWHITE, 0x0A);
    case NativeTheme::kColorId_ProminentButtonInkDropShadowColor:
      return SkColorSetA(gfx::kGoogleBlue300, 0x7F);
    case NativeTheme::kColorId_ProminentButtonHoverColor:
      return SkColorSetA(SK_ColorWHITE, 0x0A);
    case NativeTheme::kColorId_ButtonUncheckedColor:
      return gfx::kGoogleGrey500;
    case NativeTheme::kColorId_TextOnProminentButtonColor:
      return gfx::kGoogleGrey900;
    case NativeTheme::kColorId_PaddedButtonInkDropColor:
      return SK_ColorWHITE;

    // MenuItem
    case NativeTheme::kColorId_HighlightedMenuItemForegroundColor:
    case NativeTheme::kColorId_MenuDropIndicator:
      return gfx::kGoogleGrey200;
    case NativeTheme::kColorId_MenuBorderColor:
    case NativeTheme::kColorId_MenuSeparatorColor:
      return gfx::kGoogleGrey800;
    case NativeTheme::kColorId_HighlightedMenuItemBackgroundColor:
      return SkColorSetRGB(0x32, 0x36, 0x39);
    case NativeTheme::kColorId_MenuItemInitialAlertBackgroundColor:
      return SkColorSetA(gfx::kGoogleBlue300, 0x4D);
    case NativeTheme::kColorId_MenuItemTargetAlertBackgroundColor:
      return SkColorSetA(gfx::kGoogleBlue300, 0x1A);
    case NativeTheme::kColorId_MenuItemMinorTextColor:
      return gfx::kGoogleGrey500;

    // Custom frame view
    case NativeTheme::kColorId_CustomFrameActiveColor:
      return gfx::kGoogleGrey900;
    case NativeTheme::kColorId_CustomFrameInactiveColor:
      return gfx::kGoogleGrey800;

    // Custom tab bar
    case NativeTheme::kColorId_CustomTabBarBackgroundColor:
      return gfx::kGoogleGrey900;

    // Dropdown
    case NativeTheme::kColorId_DropdownBackgroundColor:
      return color_utils::AlphaBlend(SK_ColorWHITE, gfx::kGoogleGrey900, 0.04f);
    case NativeTheme::kColorId_DropdownForegroundColor:
      return gfx::kGoogleGrey200;
    case NativeTheme::kColorId_DropdownSelectedForegroundColor:
      return gfx::kGoogleGrey200;

    // Label
    case NativeTheme::kColorId_LabelEnabledColor:
    case NativeTheme::kColorId_LabelTextSelectionColor:
      return gfx::kGoogleGrey200;
    case NativeTheme::kColorId_LabelSecondaryColor:
      return gfx::kGoogleGrey500;
    case NativeTheme::kColorId_LabelTextSelectionBackgroundFocused:
      return gfx::kGoogleBlue800;

    // Link
    case NativeTheme::kColorId_LinkEnabled:
    case NativeTheme::kColorId_LinkPressed:
      return gfx::kGoogleBlue300;

    // Separator
    case NativeTheme::kColorId_SeparatorColor:
      return gfx::kGoogleGrey800;

    // TabbedPane
    case NativeTheme::kColorId_TabTitleColorActive:
    case NativeTheme::kColorId_TabSelectedBorderColor:
      return gfx::kGoogleBlue300;
    case NativeTheme::kColorId_TabTitleColorInactive:
      return gfx::kGoogleGrey500;
    case NativeTheme::kColorId_TabBottomBorder:
      return gfx::kGoogleGrey800;
    case NativeTheme::kColorId_TabHighlightBackground:
      return gfx::kGoogleGrey800;
    case NativeTheme::kColorId_TabHighlightFocusedBackground:
      return SkColorSetRGB(0x32, 0x36, 0x39);

    // Table
    case NativeTheme::kColorId_TableBackground:
    case NativeTheme::kColorId_TableBackgroundAlternate:
      return color_utils::AlphaBlend(SK_ColorWHITE, gfx::kGoogleGrey900, 0.04f);
    case NativeTheme::kColorId_TableText:
    case NativeTheme::kColorId_TableSelectedText:
    case NativeTheme::kColorId_TableSelectedTextUnfocused:
      return gfx::kGoogleGrey200;

    // Textfield
    case NativeTheme::kColorId_TextfieldDefaultColor:
    case NativeTheme::kColorId_TextfieldSelectionColor:
      return gfx::kGoogleGrey200;
    case NativeTheme::kColorId_TextfieldReadOnlyBackground: {
      return color_utils::AlphaBlend(SK_ColorWHITE, gfx::kGoogleGrey900, 0.04f);
    }
    case NativeTheme::kColorId_TextfieldSelectionBackgroundFocused:
      return gfx::kGoogleBlue800;

    // Tooltip
    case NativeTheme::kColorId_TooltipIcon:
      return SkColorSetA(gfx::kGoogleGrey200, 0xBD);
    case NativeTheme::kColorId_TooltipIconHovered:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_TooltipText:
      return SkColorSetA(gfx::kGoogleGrey200, 0xDE);

    // Tree
    case NativeTheme::kColorId_TreeBackground:
      return color_utils::AlphaBlend(SK_ColorWHITE, gfx::kGoogleGrey900, 0.04f);
    case NativeTheme::kColorId_TreeText:
    case NativeTheme::kColorId_TreeSelectedText:
    case NativeTheme::kColorId_TreeSelectedTextUnfocused:
      return gfx::kGoogleGrey200;

    // Material spinner/throbber
    case NativeTheme::kColorId_ThrobberSpinningColor:
      return gfx::kGoogleBlue300;

    case NativeTheme::kColorId_BubbleBorder:
      return gfx::kGoogleGrey800;

    case NativeTheme::kColorId_FootnoteContainerBorder:
      return gfx::kGoogleGrey900;

    // Alert icon colors
    case NativeTheme::kColorId_AlertSeverityLow:
      return gfx::kGoogleGreen300;
    case NativeTheme::kColorId_AlertSeverityMedium:
      return gfx::kGoogleYellow300;
    case NativeTheme::kColorId_AlertSeverityHigh:
      return gfx::kGoogleRed300;

    case NativeTheme::kColorId_MenuIconColor:
    case NativeTheme::kColorId_DefaultIconColor:
      return gfx::kGoogleGrey500;
    default:
      return base::nullopt;
  }
}

SkColor GetDefaultColor(NativeTheme::ColorId color_id,
                        const NativeTheme* base_theme,
                        NativeTheme::ColorScheme color_scheme) {
  constexpr SkColor kPrimaryTextColor = gfx::kGoogleGrey900;

  switch (color_id) {
    // Dialogs
    case NativeTheme::kColorId_WindowBackground:
    case NativeTheme::kColorId_ButtonColor:
    case NativeTheme::kColorId_DialogBackground:
    case NativeTheme::kColorId_BubbleBackground:
    case NativeTheme::kColorId_NotificationDefaultBackground:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_DialogForeground:
      return gfx::kGoogleGrey700;
    case NativeTheme::kColorId_BubbleForeground:
      return kPrimaryTextColor;
    case NativeTheme::kColorId_BubbleFooterBackground:
      return gfx::kGoogleGrey050;

    // Buttons
    case NativeTheme::kColorId_ButtonCheckedColor:
    case NativeTheme::kColorId_ButtonEnabledColor:
      return gfx::kGoogleBlue600;
    case NativeTheme::kColorId_ButtonInkDropShadowColor:
      return SkColorSetA(SK_ColorBLACK, 0x1A);
    case NativeTheme::kColorId_ButtonHoverColor:
      return SkColorSetA(SK_ColorBLACK, 0x05);
    case NativeTheme::kColorId_ButtonInkDropFillColor:
      return SkColorSetA(SK_ColorWHITE, 0x05);
    case NativeTheme::kColorId_ProminentButtonInkDropShadowColor:
      return SkColorSetA(SK_ColorBLACK, 0x3D);
    case NativeTheme::kColorId_ProminentButtonHoverColor:
      return SkColorSetA(SK_ColorWHITE, 0x0D);
    case NativeTheme::kColorId_ProminentButtonInkDropFillColor:
      return SkColorSetA(SK_ColorWHITE, 0x0D);
    case NativeTheme::kColorId_ProminentButtonFocusedColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.3f)
          .color;
    }
    case NativeTheme::kColorId_ProminentButtonColor:
      return gfx::kGoogleBlue600;
    case NativeTheme::kColorId_TextOnProminentButtonColor:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_ButtonUncheckedColor:
      return gfx::kGoogleGrey700;
    case NativeTheme::kColorId_ButtonDisabledColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_ButtonColor, color_scheme);
      const SkColor fg = base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
      return color_utils::BlendForMinContrast(gfx::kGoogleGrey600, bg, fg)
          .color;
    }
    case NativeTheme::kColorId_ProminentButtonDisabledColor:
    case NativeTheme::kColorId_DisabledButtonBorderColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_ButtonColor, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.2f)
          .color;
    }
    case NativeTheme::kColorId_ButtonBorderColor:
      return gfx::kGoogleGrey300;
    case NativeTheme::kColorId_PaddedButtonInkDropColor:
      return gfx::kGoogleGrey900;

    // ToggleButton
    case NativeTheme::kColorId_ToggleButtonShadowColor:
      return SkColorSetA(
          base_theme->GetSystemColor(NativeTheme::kColorId_LabelEnabledColor,
                                     color_scheme),
          0x99);
    case ui::NativeTheme::kColorId_ToggleButtonTrackColorOff:
    case ui::NativeTheme::kColorId_ToggleButtonTrackColorOn: {
      const ui::NativeTheme::ColorId base_color_id =
          color_id == ui::NativeTheme::kColorId_ToggleButtonTrackColorOff
              ? ui::NativeTheme::kColorId_LabelEnabledColor
              : ui::NativeTheme::kColorId_ProminentButtonColor;
      return SkColorSetA(
          base_theme->GetSystemColor(base_color_id, color_scheme), 0x66);
    }

    // MenuItem
    case NativeTheme::kColorId_EnabledMenuItemForegroundColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_DropdownForegroundColor, color_scheme);
    case NativeTheme::kColorId_SelectedMenuItemForegroundColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_DropdownSelectedForegroundColor, color_scheme);
    case NativeTheme::kColorId_HighlightedMenuItemForegroundColor:
    case NativeTheme::kColorId_MenuDropIndicator:
      return kPrimaryTextColor;
    case NativeTheme::kColorId_FocusedMenuItemBackgroundColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_DropdownSelectedBackgroundColor, color_scheme);
    case NativeTheme::kColorId_MenuBorderColor:
    case NativeTheme::kColorId_MenuSeparatorColor:
      return gfx::kGoogleGrey300;
    case NativeTheme::kColorId_MenuBackgroundColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_DropdownBackgroundColor, color_scheme);
    case NativeTheme::kColorId_DisabledMenuItemForegroundColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_MenuBackgroundColor, color_scheme);
      const SkColor fg = base_theme->GetSystemColor(
          NativeTheme::kColorId_EnabledMenuItemForegroundColor, color_scheme);
      return color_utils::BlendForMinContrast(gfx::kGoogleGrey600, bg, fg)
          .color;
    }
    case NativeTheme::kColorId_MenuItemMinorTextColor:
      return gfx::kGoogleGrey700;
    case NativeTheme::kColorId_HighlightedMenuItemBackgroundColor:
      return gfx::kGoogleGrey050;
    case NativeTheme::kColorId_MenuItemInitialAlertBackgroundColor:
      return SkColorSetA(gfx::kGoogleBlue600, 0x4D);
    case NativeTheme::kColorId_MenuItemTargetAlertBackgroundColor:
      return SkColorSetA(gfx::kGoogleBlue600, 0x1A);

    // Custom frame view
    case NativeTheme::kColorId_CustomFrameActiveColor:
      return SkColorSetRGB(0xDE, 0xE1, 0xE6);
    case NativeTheme::kColorId_CustomFrameInactiveColor:
      return SkColorSetRGB(0xE7, 0xEA, 0xED);

    // Custom tab bar
    case NativeTheme::kColorId_CustomTabBarBackgroundColor:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_CustomTabBarForegroundColor:
      return color_utils::GetColorWithMaxContrast(base_theme->GetSystemColor(
          NativeTheme::kColorId_CustomTabBarBackgroundColor, color_scheme));
    case NativeTheme::kColorId_CustomTabBarSecurityChipWithCertColor:
    case NativeTheme::kColorId_CustomTabBarSecurityChipSecureColor:
    case NativeTheme::kColorId_CustomTabBarSecurityChipDefaultColor:
    case NativeTheme::kColorId_CustomTabBarSecurityChipDangerousColor: {
      const SkColor fg = base_theme->GetSystemColor(
          NativeTheme::kColorId_CustomTabBarForegroundColor, color_scheme);
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_CustomTabBarBackgroundColor, color_scheme);
      return GetSecurityChipColor(GetSecurityChipColorId(color_id), fg, bg);
    }

    // Dropdown
    case NativeTheme::kColorId_DropdownBackgroundColor:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_DropdownForegroundColor:
      return kPrimaryTextColor;
    case NativeTheme::kColorId_DropdownSelectedBackgroundColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_MenuBackgroundColor, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.67f)
          .color;
    }
    case NativeTheme::kColorId_DropdownSelectedForegroundColor:
      return kPrimaryTextColor;

    // Label
    case NativeTheme::kColorId_LabelEnabledColor:
    case NativeTheme::kColorId_LabelTextSelectionColor:
      return kPrimaryTextColor;
    case NativeTheme::kColorId_LabelDisabledColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_DialogBackground, color_scheme);
      const SkColor fg = base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
      return color_utils::BlendForMinContrast(gfx::kGoogleGrey600, bg, fg)
          .color;
    }
    case NativeTheme::kColorId_LabelSecondaryColor:
      return gfx::kGoogleGrey700;
    case NativeTheme::kColorId_LabelTextSelectionBackgroundFocused:
      return gfx::kGoogleBlue200;

    // Link
    case NativeTheme::kColorId_LinkDisabled: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_DialogBackground, color_scheme);
      const SkColor fg = base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
      return color_utils::BlendForMinContrast(gfx::kGoogleGrey600, bg, fg)
          .color;
    }
    case NativeTheme::kColorId_LinkEnabled:
    case NativeTheme::kColorId_LinkPressed:
      return gfx::kGoogleBlue600;

    // Notification view
    case NativeTheme::kColorId_NotificationPlaceholderIconColor:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_NotificationActionsRowBackground:
    case NativeTheme::kColorId_NotificationInlineSettingsBackground: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_NotificationDefaultBackground, color_scheme);
      // The alpha value here (0x14) is chosen to generate 0xEEE from 0xFFF.
      return color_utils::BlendTowardMaxContrast(bg, 0x14);
    }
    case NativeTheme::kColorId_NotificationLargeImageBackground: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_NotificationDefaultBackground, color_scheme);
      // The alpha value here (0x0C) is chosen to generate 0xF5F5F5 from 0xFFF.
      return color_utils::BlendTowardMaxContrast(bg, 0x0C);
    }
    case NativeTheme::kColorId_NotificationEmptyPlaceholderIconColor:
      return SkColorSetA(SK_ColorWHITE, 0x60);
    case NativeTheme::kColorId_NotificationEmptyPlaceholderTextColor:
      return SkColorSetA(SK_ColorWHITE, gfx::kDisabledControlAlpha);
    case NativeTheme::kColorId_NotificationInkDropBase:
      return gfx::kGoogleBlue600;
#if defined(OS_CHROMEOS)
    case NativeTheme::kColorId_NotificationButtonBackground:
      return SkColorSetA(SK_ColorWHITE, 0.9 * 0xff);
#endif
    case NativeTheme::kColorId_NotificationDefaultAccentColor:
      return gfx::kChromeIconGrey;

    // Scrollbar
    case NativeTheme::kColorId_OverlayScrollbarThumbBackground:
      return SK_ColorBLACK;
    case NativeTheme::kColorId_OverlayScrollbarThumbForeground:
      return SkColorSetA(SK_ColorWHITE, (kOverlayScrollbarStrokeNormalAlpha /
                                         kOverlayScrollbarThumbNormalAlpha) *
                                            SK_AlphaOPAQUE);

    // Slider
    case NativeTheme::kColorId_SliderThumbDefault:
      return SkColorSetARGB(0xFF, 0x25, 0x81, 0xDF);
    case NativeTheme::kColorId_SliderTroughDefault:
      return SkColorSetARGB(0x40, 0x25, 0x81, 0xDF);
    case NativeTheme::kColorId_SliderThumbMinimal:
      return SkColorSetARGB(0x6E, 0xF1, 0xF3, 0xF4);
    case NativeTheme::kColorId_SliderTroughMinimal:
      return SkColorSetARGB(0x19, 0xF1, 0xF3, 0xF4);

    // Separator
    case NativeTheme::kColorId_SeparatorColor:
      return gfx::kGoogleGrey300;

    // TabbedPane
    case NativeTheme::kColorId_TabTitleColorActive:
    case NativeTheme::kColorId_TabSelectedBorderColor:
      return gfx::kGoogleBlue600;
    case NativeTheme::kColorId_TabTitleColorInactive:
      return gfx::kGoogleGrey700;
    case NativeTheme::kColorId_TabBottomBorder:
      return gfx::kGoogleGrey300;
    case NativeTheme::kColorId_TabHighlightBackground:
      return gfx::kGoogleBlue050;
    case NativeTheme::kColorId_TabHighlightFocusedBackground:
      return gfx::kGoogleBlue100;

    // Textfield
    case NativeTheme::kColorId_TextfieldDefaultColor:
    case NativeTheme::kColorId_TextfieldSelectionColor:
      return kPrimaryTextColor;
    case NativeTheme::kColorId_TextfieldDefaultBackground: {
      const SkColor fg = base_theme->GetSystemColor(
          NativeTheme::kColorId_TextfieldDefaultColor, color_scheme);
      return color_utils::GetColorWithMaxContrast(fg);
    }
    case NativeTheme::kColorId_TextfieldReadOnlyBackground:
      return SK_ColorWHITE;
    case NativeTheme::kColorId_TextfieldPlaceholderColor:
    case NativeTheme::kColorId_TextfieldReadOnlyColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_TextfieldReadOnlyBackground, color_scheme);
      const SkColor fg = base_theme->GetSystemColor(
          NativeTheme::kColorId_TextfieldDefaultColor, color_scheme);
      return color_utils::BlendForMinContrast(gfx::kGoogleGrey600, bg, fg)
          .color;
    }
    case NativeTheme::kColorId_TextfieldSelectionBackgroundFocused:
      return gfx::kGoogleBlue200;

    // Tooltip
    case NativeTheme::kColorId_TooltipBackground: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return SkColorSetA(bg, 0xCC);
    }
    case NativeTheme::kColorId_TooltipIcon:
      return SkColorSetA(gfx::kGoogleGrey800, 0xBD);
    case NativeTheme::kColorId_TooltipIconHovered:
      return SkColorSetA(SK_ColorBLACK, 0xBD);
    case NativeTheme::kColorId_TooltipText:
      return SkColorSetA(kPrimaryTextColor, 0xDE);

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
    case NativeTheme::kColorId_TableBackgroundAlternate:
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
    case NativeTheme::kColorId_UnfocusedBorderColor:
      return gfx::kGoogleGrey300;

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

    case NativeTheme::kColorId_FootnoteContainerBorder:
      return gfx::kGoogleGrey200;

    case NativeTheme::kColorId_MenuIconColor:
    case NativeTheme::kColorId_DefaultIconColor:
      return gfx::kGoogleGrey700;
    case NativeTheme::kColorId_DisabledIconColor:
      return SkColorSetA(
          base_theme->GetSystemColor(NativeTheme::kColorId_DefaultIconColor),
          gfx::kDisabledControlAlpha);

    // Sync info container
    case NativeTheme::kColorId_SyncInfoContainerPaused:
      return SkColorSetA(base_theme->GetSystemColor(
                             NativeTheme::kColorId_ProminentButtonColor),
                         16);
    case NativeTheme::kColorId_SyncInfoContainerError:
      return SkColorSetA(
          base_theme->GetSystemColor(NativeTheme::kColorId_AlertSeverityHigh),
          16);
    case NativeTheme::kColorId_SyncInfoContainerNoPrimaryAccount:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_BubbleFooterBackground);

    case NativeTheme::kColorId_BubbleBorder:
      return gfx::kGoogleGrey300;

    case NativeTheme::kColorId_NumColors:
      // Keeping the kColorId_NumColors case instead of using the default case
      // allows ColorId additions to trigger compile error for an incomplete
      // switch enumeration.
      NOTREACHED();
      return gfx::kPlaceholderColor;
  }
}

}  // namespace

SkColor GetSecurityChipColor(NativeTheme::SecurityChipColorId chip_color_id,
                             SkColor fg,
                             SkColor bg,
                             bool high_contrast) {
  const bool dark = color_utils::IsDark(bg);
  const auto blend_for_min_contrast = [&](SkColor fg, SkColor bg,
                                          base::Optional<SkColor> hc_fg =
                                              base::nullopt) {
    const float ratio =
        high_contrast ? 6.0f : color_utils::kMinimumReadableContrastRatio;
    return color_utils::BlendForMinContrast(fg, bg, hc_fg, ratio).color;
  };
  const auto security_chip_color = [&](SkColor color) {
    return blend_for_min_contrast(color, bg);
  };

  switch (chip_color_id) {
    case NativeTheme::SecurityChipColorId::DEFAULT:
    case NativeTheme::SecurityChipColorId::SECURE:
      return dark
                 ? color_utils::BlendTowardMaxContrast(fg, 0x18)
                 : security_chip_color(color_utils::DeriveDefaultIconColor(fg));
    case NativeTheme::SecurityChipColorId::DANGEROUS:
      return dark ? color_utils::BlendTowardMaxContrast(fg, 0x18)
                  : security_chip_color(gfx::kGoogleRed600);
    case NativeTheme::SecurityChipColorId::SECURE_WITH_CERT:
      return blend_for_min_contrast(fg, fg, blend_for_min_contrast(bg, bg));
    default:
      NOTREACHED();
      return gfx::kPlaceholderColor;
  }
}

SkColor GetAuraColor(NativeTheme::ColorId color_id,
                     const NativeTheme* base_theme,
                     NativeTheme::ColorScheme color_scheme) {
  if (color_scheme == NativeTheme::ColorScheme::kDefault)
    color_scheme = base_theme->GetDefaultSystemColorScheme();

  // High contrast overrides the normal colors for certain ColorIds to be much
  // darker or lighter.
  if (base_theme->UsesHighContrastColors()) {
    base::Optional<SkColor> color =
        GetHighContrastColor(color_id, color_scheme);
    if (color.has_value())
      return color.value();
  }

  if (color_scheme == NativeTheme::ColorScheme::kDark) {
    base::Optional<SkColor> color = GetDarkSchemeColor(color_id);
    if (color.has_value())
      return color.value();
  }

  return GetDefaultColor(color_id, base_theme, color_scheme);
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
