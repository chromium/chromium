// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/common_theme.h"

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/native_theme_utils.h"
#include "ui/native_theme/overlay_scrollbar_constants_aura.h"

namespace ui {

namespace {

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
    case NativeTheme::kColorId_AlertSeverityHigh:
    case NativeTheme::kColorId_AlertSeverityMedium:
      return GetAlertSeverityColor(color_id, true);

    // Bubble
    case NativeTheme::kColorId_FootnoteContainerBorder:
      return gfx::kGoogleGrey900;

    // Button
    case NativeTheme::kColorId_ProminentButtonColor:
      return gfx::kGoogleBlue300;

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
    case NativeTheme::kColorId_AlertSeverityHigh:
    case NativeTheme::kColorId_AlertSeverityMedium:
      return GetAlertSeverityColor(color_id, false);

    // Avatar
    case NativeTheme::kColorId_AvatarHeaderArt:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case NativeTheme::kColorId_AvatarIconGuest:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_AvatarIconIncognito:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);

    // Border
    case NativeTheme::kColorId_UnfocusedBorderColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case NativeTheme::kColorId_FocusedBorderColor: {
      const SkColor accent = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      return SkColorSetA(accent, 0x4D);
    }

    // Bubble
    case NativeTheme::kColorId_BubbleBackground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
    case NativeTheme::kColorId_BubbleFooterBackground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_HighlightedMenuItemBackgroundColor,
          color_scheme);
    case NativeTheme::kColorId_FootnoteContainerBorder:
      return gfx::kGoogleGrey200;
    case NativeTheme::kColorId_BubbleBorder:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);

    // Button
    case NativeTheme::kColorId_ButtonColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
    case NativeTheme::kColorId_TextOnProminentButtonColor:
      return color_utils::GetColorWithMaxContrast(
          base_theme->GetUnprocessedSystemColor(
              NativeTheme::kColorId_ProminentButtonColor, color_scheme));
    case NativeTheme::kColorId_ProminentButtonDisabledColor:
    case NativeTheme::kColorId_DisabledButtonBorderColor: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.2f)
          .color;
    }
    case NativeTheme::kColorId_ButtonBorderColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case NativeTheme::kColorId_ButtonDisabledColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelDisabledColor, color_scheme);
    case NativeTheme::kColorId_ButtonUncheckedColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_PaddedButtonInkDropColor:
      return color_utils::GetColorWithMaxContrast(
          base_theme->GetUnprocessedSystemColor(
              NativeTheme::kColorId_WindowBackground, color_scheme));
    case NativeTheme::kColorId_ProminentButtonFocusedColor: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.3f)
          .color;
    }
    case NativeTheme::kColorId_ButtonCheckedColor:
    case NativeTheme::kColorId_ButtonEnabledColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
    case NativeTheme::kColorId_ProminentButtonColor:
      return gfx::kGoogleBlue600;

    // Custom tab bar
    case NativeTheme::kColorId_CustomTabBarBackgroundColor: {
      const SkColor fg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_CustomTabBarForegroundColor, color_scheme);
      return color_utils::GetColorWithMaxContrast(fg);
    }
    case NativeTheme::kColorId_CustomTabBarForegroundColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelTextSelectionColor, color_scheme);
    case NativeTheme::kColorId_CustomTabBarSecurityChipWithCertColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelDisabledColor, color_scheme);
    case NativeTheme::kColorId_CustomTabBarSecurityChipSecureColor:
    case NativeTheme::kColorId_CustomTabBarSecurityChipDefaultColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_CustomTabBarSecurityChipDangerousColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_AlertSeverityHigh, color_scheme);

    // Dialog
    case NativeTheme::kColorId_DialogBackground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
    case NativeTheme::kColorId_DialogForeground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);

    // Dropdown
    case NativeTheme::kColorId_DropdownBackgroundColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
    case NativeTheme::kColorId_DropdownSelectedBackgroundColor: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return color_utils::BlendForMinContrast(bg, bg, base::nullopt, 1.67f)
          .color;
    }
    case NativeTheme::kColorId_DropdownForegroundColor:
    case NativeTheme::kColorId_DropdownSelectedForegroundColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);

    // Frame
    case NativeTheme::kColorId_CustomFrameActiveColor:
      return SkColorSetRGB(0xDE, 0xE1, 0xE6);
    case NativeTheme::kColorId_CustomFrameInactiveColor:
      return SkColorSetRGB(0xE7, 0xEA, 0xED);

    // Icon
    case NativeTheme::kColorId_DefaultIconColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_DisabledIconColor: {
      const SkColor icon = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
      return SkColorSetA(icon, gfx::kDisabledControlAlpha);
    }

    // Label
    case NativeTheme::kColorId_LabelDisabledColor: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      const SkColor fg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
      return color_utils::BlendForMinContrast(gfx::kGoogleGrey600, bg, fg)
          .color;
    }
    case NativeTheme::kColorId_LabelSecondaryColor:
      return gfx::kGoogleGrey700;
    case NativeTheme::kColorId_LabelEnabledColor:
      return gfx::kGoogleGrey900;
    case NativeTheme::kColorId_LabelTextSelectionColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
    case NativeTheme::kColorId_LabelTextSelectionBackgroundFocused:
      return gfx::kGoogleBlue200;

    // Link
    case NativeTheme::kColorId_LinkDisabled:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelDisabledColor, color_scheme);
    case NativeTheme::kColorId_LinkEnabled:
    case NativeTheme::kColorId_LinkPressed:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);

    // Menu
    case NativeTheme::kColorId_MenuBackgroundColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
    case NativeTheme::kColorId_HighlightedMenuItemBackgroundColor:
      return gfx::kGoogleGrey050;
    case NativeTheme::kColorId_MenuBorderColor:
    case NativeTheme::kColorId_MenuSeparatorColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case NativeTheme::kColorId_FocusedMenuItemBackgroundColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_DropdownSelectedBackgroundColor, color_scheme);
    case NativeTheme::kColorId_DisabledMenuItemForegroundColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelDisabledColor, color_scheme);
    case NativeTheme::kColorId_MenuIconColor:
    case NativeTheme::kColorId_MenuItemMinorTextColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_EnabledMenuItemForegroundColor:
    case NativeTheme::kColorId_HighlightedMenuItemForegroundColor:
    case NativeTheme::kColorId_MenuDropIndicator:
    case NativeTheme::kColorId_SelectedMenuItemForegroundColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
    case NativeTheme::kColorId_MenuItemInitialAlertBackgroundColor:
    case NativeTheme::kColorId_MenuItemTargetAlertBackgroundColor: {
      const SkColor accent = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      constexpr auto kInitial =
          NativeTheme::kColorId_MenuItemInitialAlertBackgroundColor;
      return SkColorSetA(
          accent, (color_id == kInitial) ? 0x4D : gfx::kGoogleGreyAlpha200);
    }

    // Notification
    case NativeTheme::kColorId_NotificationBackground:
    case NativeTheme::kColorId_NotificationColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case NativeTheme::kColorId_NotificationButtonBackground:
      return SkColorSetA(SK_ColorWHITE, 0.9 * 0xff);
#endif
    case NativeTheme::kColorId_NotificationPlaceholderColor:
      return SkColorSetA(SK_ColorWHITE, gfx::kDisabledControlAlpha);
    case NativeTheme::kColorId_NotificationActionsRowBackground:
    case NativeTheme::kColorId_NotificationBackgroundActive: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return color_utils::BlendTowardMaxContrast(bg, 0x14);
    }
    case NativeTheme::kColorId_NotificationLargeImageBackground: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return color_utils::BlendTowardMaxContrast(bg, 0x0C);
    }
    case NativeTheme::kColorId_NotificationDefaultAccentColor:
      return gfx::kGoogleGrey700;
    case NativeTheme::kColorId_NotificationInkDropBase:
      return gfx::kGoogleBlue600;

    // Scrollbar
    case NativeTheme::kColorId_OverlayScrollbarThumbFill:
    case NativeTheme::kColorId_OverlayScrollbarThumbHoveredFill: {
      SkColor fill = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_CustomTabBarForegroundColor, color_scheme);
      fill = color_utils::IsDark(fill) ? SK_ColorBLACK : SK_ColorWHITE;
      const bool hovered =
          color_id == NativeTheme::kColorId_OverlayScrollbarThumbHoveredFill;
      return SkColorSetA(fill, (hovered ? 0.7 : 0.5) * SK_AlphaOPAQUE);
    }
    case NativeTheme::kColorId_OverlayScrollbarThumbStroke:
    case NativeTheme::kColorId_OverlayScrollbarThumbHoveredStroke: {
      SkColor stroke = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_CustomTabBarBackgroundColor, color_scheme);
      stroke = color_utils::IsDark(stroke) ? SK_ColorBLACK : SK_ColorWHITE;
      const bool hovered =
          color_id == NativeTheme::kColorId_OverlayScrollbarThumbHoveredStroke;
      return SkColorSetA(stroke, (hovered ? 0.5 : 0.3) * SK_AlphaOPAQUE);
    }

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
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_HighlightedMenuItemBackgroundColor,
          color_scheme);
    case NativeTheme::kColorId_SyncInfoContainerPaused:
      return SkColorSetA(
          base_theme->GetUnprocessedSystemColor(
              NativeTheme::kColorId_ProminentButtonColor, color_scheme),
          0x10);
    case NativeTheme::kColorId_SyncInfoContainerError:
      return SkColorSetA(
          base_theme->GetUnprocessedSystemColor(
              NativeTheme::kColorId_AlertSeverityHigh, color_scheme),
          0x10);

    // Tabbed pane
    case NativeTheme::kColorId_TabBottomBorder:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case NativeTheme::kColorId_TabTitleColorInactive:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_TabHighlightBackground:
      return gfx::kGoogleBlue050;
    case NativeTheme::kColorId_TabHighlightFocusedBackground:
      return gfx::kGoogleBlue100;
    case NativeTheme::kColorId_TabTitleColorActive:
    case NativeTheme::kColorId_TabSelectedBorderColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);

    // Table
    case NativeTheme::kColorId_TableBackground:
    case NativeTheme::kColorId_TableBackgroundAlternate:
    case NativeTheme::kColorId_TableHeaderBackground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
    case NativeTheme::kColorId_TableGroupingIndicatorColor:
    case NativeTheme::kColorId_TableSelectionBackgroundFocused:
    case NativeTheme::kColorId_TableSelectionBackgroundUnfocused:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_DropdownSelectedBackgroundColor, color_scheme);
    case NativeTheme::kColorId_TableHeaderSeparator:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case NativeTheme::kColorId_TableHeaderText:
    case NativeTheme::kColorId_TableSelectedText:
    case NativeTheme::kColorId_TableSelectedTextUnfocused:
    case NativeTheme::kColorId_TableText:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);

    // Textfield
    case NativeTheme::kColorId_TextfieldReadOnlyBackground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
    case NativeTheme::kColorId_TextfieldDefaultBackground: {
      const SkColor fg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
      return color_utils::GetColorWithMaxContrast(fg);
    }
    case NativeTheme::kColorId_TextfieldPlaceholderColor:
    case NativeTheme::kColorId_TextfieldReadOnlyColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelDisabledColor, color_scheme);
    case NativeTheme::kColorId_TextfieldDefaultColor:
    case NativeTheme::kColorId_TextfieldSelectionColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
    case NativeTheme::kColorId_TextfieldSelectionBackgroundFocused:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelTextSelectionBackgroundFocused,
          color_scheme);

    // Throbber
    case NativeTheme::kColorId_ThrobberWaitingColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelTextSelectionBackgroundFocused,
          color_scheme);
    case NativeTheme::kColorId_ThrobberSpinningColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);

    // Toggle button
    case NativeTheme::kColorId_ToggleButtonShadowColor:
      return SkColorSetA(
          base_theme->GetUnprocessedSystemColor(
              NativeTheme::kColorId_LabelEnabledColor, color_scheme),
          0x99);
    case ui::NativeTheme::kColorId_ToggleButtonTrackColorOff:
    case ui::NativeTheme::kColorId_ToggleButtonTrackColorOn: {
      const ui::NativeTheme::ColorId base_color_id =
          color_id == ui::NativeTheme::kColorId_ToggleButtonTrackColorOff
              ? ui::NativeTheme::kColorId_LabelEnabledColor
              : ui::NativeTheme::kColorId_ProminentButtonColor;
      return SkColorSetA(
          base_theme->GetUnprocessedSystemColor(base_color_id, color_scheme),
          0x66);
    }

    // Tooltip
    case NativeTheme::kColorId_TooltipBackground: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return SkColorSetA(bg, 0xCC);
    }
    case NativeTheme::kColorId_TooltipIcon:
      return SkColorSetA(gfx::kGoogleGrey800, 0xBD);
    case NativeTheme::kColorId_TooltipText: {
      const SkColor text = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
      return SkColorSetA(text, 0xDE);
    }
    case NativeTheme::kColorId_TooltipIconHovered:
      return SkColorSetA(SK_ColorBLACK, 0xBD);

    // Tree
    case NativeTheme::kColorId_TreeBackground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
    case NativeTheme::kColorId_TreeSelectionBackgroundFocused:
    case NativeTheme::kColorId_TreeSelectionBackgroundUnfocused:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_DropdownSelectedBackgroundColor, color_scheme);
    case NativeTheme::kColorId_TreeSelectedText:
    case NativeTheme::kColorId_TreeSelectedTextUnfocused:
    case NativeTheme::kColorId_TreeText:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);

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

SkColor GetAlertSeverityColor(NativeTheme::ColorId color_id, bool dark) {
  constexpr auto kColorIdMap =
      base::MakeFixedFlatMap<NativeTheme::ColorId, std::array<SkColor, 2>>({
          {NativeTheme::kColorId_AlertSeverityHigh,
           {{gfx::kGoogleRed600, gfx::kGoogleRed300}}},
          {NativeTheme::kColorId_AlertSeverityLow,
           {{gfx::kGoogleGreen700, gfx::kGoogleGreen300}}},
          {NativeTheme::kColorId_AlertSeverityMedium,
           {{gfx::kGoogleYellow700, gfx::kGoogleYellow300}}},
      });
  return kColorIdMap.at(color_id)[dark];
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
    if (color.has_value()) {
      DVLOG(2) << "GetHighContrastColor: "
               << "NativeTheme::ColorId: " << NativeThemeColorIdName(color_id)
               << " Color: " << SkColorName(color.value());
      return color.value();
    }
  }

  if (color_scheme == NativeTheme::ColorScheme::kDark) {
    base::Optional<SkColor> color = GetDarkSchemeColor(color_id);
    if (color.has_value()) {
      DVLOG(2) << "GetDarkSchemeColor: "
               << "NativeTheme::ColorId: " << NativeThemeColorIdName(color_id)
               << " Color: " << SkColorName(color.value());
      return color.value();
    }
  }

  SkColor color = GetDefaultColor(color_id, base_theme, color_scheme);
  DVLOG(2) << "GetDefaultColor: "
           << "NativeTheme::ColorId: " << NativeThemeColorIdName(color_id)
           << " Color: " << SkColorName(color);
  return color;
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
