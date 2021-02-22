// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/common_theme.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
    // Alert
    case NativeTheme::kColorId_AlertSeverityLow:
      return gfx::kGoogleGreen300;
    case NativeTheme::kColorId_AlertSeverityHigh:
      return gfx::kGoogleRed300;
    case NativeTheme::kColorId_AlertSeverityMedium:
      return gfx::kGoogleYellow300;

    // Bubble
    case NativeTheme::kColorId_FootnoteContainerBorder:
      return gfx::kGoogleGrey900;

    // Button
    case NativeTheme::kColorId_ButtonInkDropShadowColor:
      return SkColorSetA(SK_ColorBLACK, 0x7F);
    case NativeTheme::kColorId_ButtonHoverColor:
      return SkColorSetA(SK_ColorBLACK, 0x0A);
    case NativeTheme::kColorId_ButtonInkDropFillColor:
    case NativeTheme::kColorId_ProminentButtonHoverColor:
      return SkColorSetA(SK_ColorWHITE, 0x0A);
    case NativeTheme::kColorId_ProminentButtonColor:
      return gfx::kGoogleBlue300;
    case NativeTheme::kColorId_ProminentButtonInkDropShadowColor:
      return SkColorSetA(gfx::kGoogleBlue300, 0x7F);

    // Custom tab bar
    case NativeTheme::kColorId_CustomTabBarBackgroundColor:
      return gfx::kGoogleGrey900;

    // Frame
    case NativeTheme::kColorId_CustomFrameActiveColor:
      return gfx::kGoogleGrey900;
    case NativeTheme::kColorId_CustomFrameInactiveColor:
      return gfx::kGoogleGrey800;

    // Label
    case NativeTheme::kColorId_LabelSecondaryColor:
      return gfx::kGoogleGrey500;
    case NativeTheme::kColorId_LabelEnabledColor:
      return gfx::kGoogleGrey200;
    case NativeTheme::kColorId_LabelTextSelectionBackgroundFocused:
      return gfx::kGoogleBlue800;

    // Menu
    case NativeTheme::kColorId_HighlightedMenuItemBackgroundColor:
      return SkColorSetRGB(0x32, 0x36, 0x39);

    // Separator
    case NativeTheme::kColorId_SeparatorColor:
      return gfx::kGoogleGrey800;

    // Tabbed pane
    case NativeTheme::kColorId_TabHighlightFocusedBackground:
      return SkColorSetRGB(0x32, 0x36, 0x39);
    case NativeTheme::kColorId_TabHighlightBackground:
      return gfx::kGoogleGrey800;

    // Tooltip
    case NativeTheme::kColorId_TooltipIcon:
      return SkColorSetA(gfx::kGoogleGrey200, 0xBD);
    case NativeTheme::kColorId_TooltipIconHovered:
      return SK_ColorWHITE;

    // Window
    case NativeTheme::kColorId_WindowBackground:
      return color_utils::AlphaBlend(SK_ColorWHITE, gfx::kGoogleGrey900, 0.04f);

    default:
      return base::nullopt;
  }
}

SkColor GetDefaultColor(NativeTheme::ColorId color_id,
                        const NativeTheme* base_theme,
                        NativeTheme::ColorScheme color_scheme) {
  switch (color_id) {
    // Alert
    case NativeTheme::kColorId_AlertSeverityLow:
      return gfx::kGoogleGreen700;
    case NativeTheme::kColorId_AlertSeverityHigh:
      return gfx::kGoogleRed600;
    case NativeTheme::kColorId_AlertSeverityMedium:
      return gfx::kGoogleYellow700;

    // Avatar
    case NativeTheme::kColorId_AvatarHeaderArt:
      return base_theme->GetSystemColor(NativeTheme::kColorId_SeparatorColor,
                                        color_scheme);
    case NativeTheme::kColorId_AvatarIconGuest:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_AvatarIconIncognito:
      return base_theme->GetSystemColor(NativeTheme::kColorId_LabelEnabledColor,
                                        color_scheme);

    // Border
    case NativeTheme::kColorId_UnfocusedBorderColor:
      return base_theme->GetSystemColor(NativeTheme::kColorId_SeparatorColor,
                                        color_scheme);
    case NativeTheme::kColorId_FocusedBorderColor: {
      const SkColor accent = base_theme->GetSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      return SkColorSetA(accent, 0x4D);
    }

    // Bubble
    case NativeTheme::kColorId_BubbleBackground:
      return base_theme->GetSystemColor(NativeTheme::kColorId_WindowBackground,
                                        color_scheme);
    case NativeTheme::kColorId_BubbleFooterBackground:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_HighlightedMenuItemBackgroundColor,
          color_scheme);
    case NativeTheme::kColorId_FootnoteContainerBorder:
      return gfx::kGoogleGrey200;
    case NativeTheme::kColorId_BubbleBorder:
      return base_theme->GetSystemColor(NativeTheme::kColorId_SeparatorColor,
                                        color_scheme);

    // Button
    case NativeTheme::kColorId_ButtonColor:
      return base_theme->GetSystemColor(NativeTheme::kColorId_WindowBackground,
                                        color_scheme);
    case NativeTheme::kColorId_TextOnProminentButtonColor:
      return color_utils::GetColorWithMaxContrast(base_theme->GetSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme));
    case NativeTheme::kColorId_ProminentButtonHoverColor:
      return SkColorSetA(SK_ColorWHITE, 0x0D);
    case NativeTheme::kColorId_ProminentButtonInkDropFillColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_ProminentButtonHoverColor, color_scheme);
    case NativeTheme::kColorId_ButtonInkDropFillColor:
      return SkColorSetA(SK_ColorWHITE, 0x05);
    case NativeTheme::kColorId_ProminentButtonDisabledColor:
    case NativeTheme::kColorId_DisabledButtonBorderColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.2f)
          .color;
    }
    case NativeTheme::kColorId_ButtonBorderColor:
      return base_theme->GetSystemColor(NativeTheme::kColorId_SeparatorColor,
                                        color_scheme);
    case NativeTheme::kColorId_ButtonDisabledColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelDisabledColor, color_scheme);
    case NativeTheme::kColorId_ButtonUncheckedColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_PaddedButtonInkDropColor:
      return color_utils::GetColorWithMaxContrast(base_theme->GetSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme));
    case NativeTheme::kColorId_ButtonHoverColor:
      return SkColorSetA(SK_ColorBLACK, 0x05);
    case NativeTheme::kColorId_ButtonInkDropShadowColor:
      return SkColorSetA(SK_ColorBLACK, gfx::kGoogleGreyAlpha200);
    case NativeTheme::kColorId_ProminentButtonInkDropShadowColor:
      return SkColorSetA(SK_ColorBLACK, 0x3D);
    case NativeTheme::kColorId_ProminentButtonFocusedColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.3f)
          .color;
    }
    case NativeTheme::kColorId_ButtonCheckedColor:
    case NativeTheme::kColorId_ButtonEnabledColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
    case NativeTheme::kColorId_ProminentButtonColor:
      return gfx::kGoogleBlue600;

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

    // Dialog
    case NativeTheme::kColorId_DialogBackground:
      return base_theme->GetSystemColor(NativeTheme::kColorId_WindowBackground,
                                        color_scheme);
    case NativeTheme::kColorId_DialogForeground:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);

    // Dropdown
    case NativeTheme::kColorId_DropdownBackgroundColor:
      return base_theme->GetSystemColor(NativeTheme::kColorId_WindowBackground,
                                        color_scheme);
    case NativeTheme::kColorId_DropdownSelectedBackgroundColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.67f)
          .color;
    }
    case NativeTheme::kColorId_DropdownForegroundColor:
    case NativeTheme::kColorId_DropdownSelectedForegroundColor:
      return base_theme->GetSystemColor(NativeTheme::kColorId_LabelEnabledColor,
                                        color_scheme);

    // Frame
    case NativeTheme::kColorId_CustomFrameActiveColor:
      return SkColorSetRGB(0xDE, 0xE1, 0xE6);
    case NativeTheme::kColorId_CustomFrameInactiveColor:
      return SkColorSetRGB(0xE7, 0xEA, 0xED);

    // Icon
    case NativeTheme::kColorId_DefaultIconColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_DisabledIconColor: {
      const SkColor icon = base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
      return SkColorSetA(icon, gfx::kDisabledControlAlpha);
    }

    // Label
    case NativeTheme::kColorId_LabelDisabledColor: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      const SkColor fg = base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
      return color_utils::BlendForMinContrast(gfx::kGoogleGrey600, bg, fg)
          .color;
    }
    case NativeTheme::kColorId_LabelSecondaryColor:
      return gfx::kGoogleGrey700;
    case NativeTheme::kColorId_LabelEnabledColor:
      return gfx::kGoogleGrey900;
    case NativeTheme::kColorId_LabelTextSelectionColor:
      return base_theme->GetSystemColor(NativeTheme::kColorId_LabelEnabledColor,
                                        color_scheme);
    case NativeTheme::kColorId_LabelTextSelectionBackgroundFocused:
      return gfx::kGoogleBlue200;

    // Link
    case NativeTheme::kColorId_LinkDisabled:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelDisabledColor, color_scheme);
    case NativeTheme::kColorId_LinkEnabled:
    case NativeTheme::kColorId_LinkPressed:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);

    // Menu
    case NativeTheme::kColorId_MenuBackgroundColor:
      return base_theme->GetSystemColor(NativeTheme::kColorId_WindowBackground,
                                        color_scheme);
    case NativeTheme::kColorId_HighlightedMenuItemBackgroundColor:
      return gfx::kGoogleGrey050;
    case NativeTheme::kColorId_MenuBorderColor:
    case NativeTheme::kColorId_MenuSeparatorColor:
      return base_theme->GetSystemColor(NativeTheme::kColorId_SeparatorColor,
                                        color_scheme);
    case NativeTheme::kColorId_FocusedMenuItemBackgroundColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_DropdownSelectedBackgroundColor, color_scheme);
    case NativeTheme::kColorId_DisabledMenuItemForegroundColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelDisabledColor, color_scheme);
    case NativeTheme::kColorId_MenuIconColor:
    case NativeTheme::kColorId_MenuItemMinorTextColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_EnabledMenuItemForegroundColor:
    case NativeTheme::kColorId_HighlightedMenuItemForegroundColor:
    case NativeTheme::kColorId_MenuDropIndicator:
    case NativeTheme::kColorId_SelectedMenuItemForegroundColor:
      return base_theme->GetSystemColor(NativeTheme::kColorId_LabelEnabledColor,
                                        color_scheme);
    case NativeTheme::kColorId_MenuItemInitialAlertBackgroundColor:
    case NativeTheme::kColorId_MenuItemTargetAlertBackgroundColor: {
      const SkColor accent = base_theme->GetSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      constexpr auto kInitial =
          NativeTheme::kColorId_MenuItemInitialAlertBackgroundColor;
      return SkColorSetA(
          accent, (color_id == kInitial) ? 0x4D : gfx::kGoogleGreyAlpha200);
    }

    // Notification
    case NativeTheme::kColorId_NotificationDefaultBackground:
    case NativeTheme::kColorId_NotificationPlaceholderIconColor:
      return base_theme->GetSystemColor(NativeTheme::kColorId_WindowBackground,
                                        color_scheme);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case NativeTheme::kColorId_NotificationButtonBackground:
      return SkColorSetA(SK_ColorWHITE, 0.9 * 0xff);
#endif
    case NativeTheme::kColorId_NotificationEmptyPlaceholderTextColor:
      return SkColorSetA(SK_ColorWHITE, gfx::kDisabledControlAlpha);
    case NativeTheme::kColorId_NotificationEmptyPlaceholderIconColor:
      return SkColorSetA(SK_ColorWHITE, 0x60);
    case NativeTheme::kColorId_NotificationActionsRowBackground:
    case NativeTheme::kColorId_NotificationInlineSettingsBackground: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return color_utils::BlendTowardMaxContrast(bg, 0x14);
    }
    case NativeTheme::kColorId_NotificationLargeImageBackground: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return color_utils::BlendTowardMaxContrast(bg, 0x0C);
    }
    case NativeTheme::kColorId_NotificationDefaultAccentColor:
      return gfx::kGoogleGrey700;
    case NativeTheme::kColorId_NotificationInkDropBase:
      return gfx::kGoogleBlue600;

    // Scrollbar
    case NativeTheme::kColorId_OverlayScrollbarThumbForeground:
      return SkColorSetA(SK_ColorWHITE, (kOverlayScrollbarStrokeNormalAlpha /
                                         kOverlayScrollbarThumbNormalAlpha) *
                                            SK_AlphaOPAQUE);
    case NativeTheme::kColorId_OverlayScrollbarThumbBackground:
      return SK_ColorBLACK;

    // Separator
    case NativeTheme::kColorId_SeparatorColor:
      return gfx::kGoogleGrey300;

    // Slider
    case NativeTheme::kColorId_SliderThumbMinimal:
      return SkColorSetA(gfx::kGoogleGrey100, gfx::kGoogleGreyAlpha500);
    case NativeTheme::kColorId_SliderTroughMinimal:
      return SkColorSetA(gfx::kGoogleGrey100, 0x19);
    case NativeTheme::kColorId_SliderThumbDefault:
      return gfx::kGoogleBlueDark600;
    case NativeTheme::kColorId_SliderTroughDefault:
      return SkColorSetA(gfx::kGoogleBlueDark600, 0x40);

    // Sync info container
    case NativeTheme::kColorId_SyncInfoContainerNoPrimaryAccount:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_HighlightedMenuItemBackgroundColor,
          color_scheme);
    case NativeTheme::kColorId_SyncInfoContainerPaused:
      return SkColorSetA(
          base_theme->GetSystemColor(NativeTheme::kColorId_ProminentButtonColor,
                                     color_scheme),
          0x10);
    case NativeTheme::kColorId_SyncInfoContainerError:
      return SkColorSetA(
          base_theme->GetSystemColor(NativeTheme::kColorId_AlertSeverityHigh,
                                     color_scheme),
          0x10);

    // Tabbed pane
    case NativeTheme::kColorId_TabBottomBorder:
      return base_theme->GetSystemColor(NativeTheme::kColorId_SeparatorColor,
                                        color_scheme);
    case NativeTheme::kColorId_TabTitleColorInactive:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_TabHighlightBackground:
      return gfx::kGoogleBlue050;
    case NativeTheme::kColorId_TabHighlightFocusedBackground:
      return gfx::kGoogleBlue100;
    case NativeTheme::kColorId_TabTitleColorActive:
    case NativeTheme::kColorId_TabSelectedBorderColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);

    // Table
    case NativeTheme::kColorId_TableBackground:
    case NativeTheme::kColorId_TableBackgroundAlternate:
    case NativeTheme::kColorId_TableHeaderBackground:
      return base_theme->GetSystemColor(NativeTheme::kColorId_WindowBackground,
                                        color_scheme);
    case NativeTheme::kColorId_TableGroupingIndicatorColor:
    case NativeTheme::kColorId_TableSelectionBackgroundFocused:
    case NativeTheme::kColorId_TableSelectionBackgroundUnfocused:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_DropdownSelectedBackgroundColor, color_scheme);
    case NativeTheme::kColorId_TableHeaderSeparator:
      return base_theme->GetSystemColor(NativeTheme::kColorId_SeparatorColor,
                                        color_scheme);
    case NativeTheme::kColorId_TableHeaderText:
    case NativeTheme::kColorId_TableSelectedText:
    case NativeTheme::kColorId_TableSelectedTextUnfocused:
    case NativeTheme::kColorId_TableText:
      return base_theme->GetSystemColor(NativeTheme::kColorId_LabelEnabledColor,
                                        color_scheme);

    // Textfield
    case NativeTheme::kColorId_TextfieldReadOnlyBackground:
      return base_theme->GetSystemColor(NativeTheme::kColorId_WindowBackground,
                                        color_scheme);
    case NativeTheme::kColorId_TextfieldDefaultBackground: {
      const SkColor fg = base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
      return color_utils::GetColorWithMaxContrast(fg);
    }
    case NativeTheme::kColorId_TextfieldPlaceholderColor:
    case NativeTheme::kColorId_TextfieldReadOnlyColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelDisabledColor, color_scheme);
    case NativeTheme::kColorId_TextfieldDefaultColor:
    case NativeTheme::kColorId_TextfieldSelectionColor:
      return base_theme->GetSystemColor(NativeTheme::kColorId_LabelEnabledColor,
                                        color_scheme);
    case NativeTheme::kColorId_TextfieldSelectionBackgroundFocused:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelTextSelectionBackgroundFocused,
          color_scheme);

    // Throbber
    case NativeTheme::kColorId_ThrobberLightColor:
      return SkColorSetRGB(0xF4, 0xF8, 0xFD);
    case NativeTheme::kColorId_ThrobberWaitingColor:
      return SkColorSetRGB(0xA6, 0xA6, 0xA6);
    case NativeTheme::kColorId_ThrobberSpinningColor:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);

    // Toggle button
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

    // Tooltip
    case NativeTheme::kColorId_TooltipBackground: {
      const SkColor bg = base_theme->GetSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return SkColorSetA(bg, 0xCC);
    }
    case NativeTheme::kColorId_TooltipIcon:
      return SkColorSetA(gfx::kGoogleGrey800, 0xBD);
    case NativeTheme::kColorId_TooltipText: {
      const SkColor text = base_theme->GetSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
      return SkColorSetA(text, 0xDE);
    }
    case NativeTheme::kColorId_TooltipIconHovered:
      return SkColorSetA(SK_ColorBLACK, 0xBD);

    // Tree
    case NativeTheme::kColorId_TreeBackground:
      return base_theme->GetSystemColor(NativeTheme::kColorId_WindowBackground,
                                        color_scheme);
    case NativeTheme::kColorId_TreeSelectionBackgroundFocused:
    case NativeTheme::kColorId_TreeSelectionBackgroundUnfocused:
      return base_theme->GetSystemColor(
          NativeTheme::kColorId_DropdownSelectedBackgroundColor, color_scheme);
    case NativeTheme::kColorId_TreeSelectedText:
    case NativeTheme::kColorId_TreeSelectedTextUnfocused:
    case NativeTheme::kColorId_TreeText:
      return base_theme->GetSystemColor(NativeTheme::kColorId_LabelEnabledColor,
                                        color_scheme);

    // Window
    case NativeTheme::kColorId_WindowBackground:
      return SK_ColorWHITE;

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
  if (base_theme->UserHasContrastPreference()) {
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
