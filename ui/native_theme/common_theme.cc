// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/common_theme.h"

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

absl::optional<SkColor> GetHighContrastColor(
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
      return absl::nullopt;
  }
}

absl::optional<SkColor> GetDarkSchemeColor(NativeTheme::ColorId color_id,
                                           const NativeTheme* base_theme) {
  switch (color_id) {
    // Alert
    case NativeTheme::kColorId_AlertSeverityLow:
    case NativeTheme::kColorId_AlertSeverityHigh:
    case NativeTheme::kColorId_AlertSeverityMedium: {
      auto provider_color_id = NativeThemeColorIdToColorId(color_id);
      DCHECK(provider_color_id);
      return GetAlertSeverityColor(provider_color_id.value(), true);
    }

    // Border
    case NativeTheme::kColorId_FocusedBorderColor:
      return gfx::kGoogleBlue400;

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

    // Shadow
    case NativeTheme::kColorId_ShadowBase:
      return SK_ColorBLACK;

    // Separator
    case NativeTheme::kColorId_SeparatorColor:
      return gfx::kGoogleGrey800;

    // Toggle button
    case ui::NativeTheme::kColorId_ToggleButtonThumbColorOff: {
      const SkColor enabled = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor,
          NativeTheme::ColorScheme::kDark);
      const SkColor secondary = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor,
          NativeTheme::ColorScheme::kDark);
      return color_utils::AlphaBlend(enabled, secondary, SkAlpha{0x0D});
    }
    case ui::NativeTheme::kColorId_ToggleButtonThumbColorOn: {
      const SkColor enabled = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor,
          NativeTheme::ColorScheme::kDark);
      const SkColor prominent = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor,
          NativeTheme::ColorScheme::kDark);
      return color_utils::AlphaBlend(enabled, prominent, SkAlpha{0x0D});
    }
    case ui::NativeTheme::kColorId_ToggleButtonTrackColorOff:
      return gfx::kGoogleGrey700;
    case ui::NativeTheme::kColorId_ToggleButtonTrackColorOn:
      return gfx::kGoogleBlue600;

    // Window
    case NativeTheme::kColorId_WindowBackground:
      return color_utils::BlendTowardMaxContrast(gfx::kGoogleGrey900, 0x0A);

    default:
      return absl::nullopt;
  }
}

SkColor GetDefaultColor(NativeTheme::ColorId color_id,
                        const NativeTheme* base_theme,
                        NativeTheme::ColorScheme color_scheme) {
  switch (color_id) {
    // Alert
    case NativeTheme::kColorId_AlertSeverityLow:
    case NativeTheme::kColorId_AlertSeverityHigh:
    case NativeTheme::kColorId_AlertSeverityMedium: {
      auto provider_color_id = NativeThemeColorIdToColorId(color_id);
      DCHECK(provider_color_id);
      return GetAlertSeverityColor(provider_color_id.value(), false);
    }

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
    case NativeTheme::kColorId_FocusedBorderColor:
      return gfx::kGoogleBlue500;

    // Bubble
    case NativeTheme::kColorId_BubbleBackground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
    case NativeTheme::kColorId_BubbleFooterBackground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_HighlightedMenuItemBackgroundColor,
          color_scheme);
    case NativeTheme::kColorId_FootnoteContainerBorder:
    case NativeTheme::kColorId_BubbleBorder:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case NativeTheme::kColorId_BubbleBorderShadowLarge:
      return SkColorSetA(base_theme->GetUnprocessedSystemColor(
                             NativeTheme::kColorId_ShadowBase, color_scheme),
                         0x1A);
    case NativeTheme::kColorId_BubbleBorderShadowSmall:
      return SkColorSetA(base_theme->GetUnprocessedSystemColor(
                             NativeTheme::kColorId_ShadowBase, color_scheme),
                         0x33);
    case NativeTheme::kColorId_BubbleBorderWhenShadowPresent:
      return SkColorSetA(SK_ColorBLACK, 0x26);
    // Button
    case NativeTheme::kColorId_ButtonColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
    case NativeTheme::kColorId_TextOnProminentButtonColor:
      return color_utils::GetColorWithMaxContrast(
          base_theme->GetUnprocessedSystemColor(
              NativeTheme::kColorId_ProminentButtonColor, color_scheme));
    case NativeTheme::kColorId_ProminentButtonDisabledColor:
    case NativeTheme::kColorId_DisabledButtonBorderColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_HighlightedMenuItemBackgroundColor,
          color_scheme);
    case NativeTheme::kColorId_ButtonBorderColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case NativeTheme::kColorId_ButtonDisabledColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelDisabledColor, color_scheme);
    case NativeTheme::kColorId_ButtonUncheckedColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_ButtonCheckedColor:
    case NativeTheme::kColorId_ButtonEnabledColor:
    case NativeTheme::kColorId_ProminentButtonFocusedColor:
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
    case NativeTheme::kColorId_CustomTabBarForegroundColor: {
      const SkColor fg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return color_utils::GetColorWithMaxContrast(fg);
    }
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
    case NativeTheme::kColorId_DropdownSelectedBackgroundColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_FocusedMenuItemBackgroundColor, color_scheme);
    case NativeTheme::kColorId_DropdownForegroundColor:
    case NativeTheme::kColorId_DropdownSelectedForegroundColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);

    // Frame
    case NativeTheme::kColorId_CustomFrameActiveColor:
      return SkColorSetRGB(0xDE, 0xE1, 0xE6);
    case NativeTheme::kColorId_CustomFrameInactiveColor:
      return gfx::kGoogleGrey200;

    // Icon
    case NativeTheme::kColorId_DefaultIconColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_DisabledIconColor: {
      const SkColor icon = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_DefaultIconColor, color_scheme);
      return SkColorSetA(icon, gfx::kDisabledControlAlpha);
    }
    case NativeTheme::kColorId_SecondaryIconColor:
      return gfx::kGoogleGrey600;

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
    case NativeTheme::kColorId_LabelTextSelectionColor: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelTextSelectionBackgroundFocused,
          color_scheme);
      return color_utils::GetColorWithMaxContrast(bg);
    }
    case NativeTheme::kColorId_LabelTextSelectionBackgroundFocused: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      const SkColor fg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      return color_utils::AlphaBlend(fg, bg, gfx::kGoogleGreyAlpha500);
    }

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
    case NativeTheme::kColorId_HighlightedMenuItemBackgroundColor: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return color_utils::BlendTowardMaxContrast(bg, gfx::kGoogleGreyAlpha100);
    }
    case NativeTheme::kColorId_MenuBorderColor:
    case NativeTheme::kColorId_MenuSeparatorColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case NativeTheme::kColorId_FocusedMenuItemBackgroundColor: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      const SkColor fg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_CustomTabBarForegroundColor, color_scheme);
      return color_utils::AlphaBlend(fg, bg, gfx::kGoogleGreyAlpha200);
    }
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
    case NativeTheme::kColorId_MessageCenterSmallImageMaskForeground:
    case NativeTheme::kColorId_NotificationBackground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case NativeTheme::kColorId_NotificationButtonBackground:
      return SkColorSetA(SK_ColorWHITE, 0.9 * 0xff);
#endif
    case NativeTheme::kColorId_NotificationPlaceholderColor: {
      const SkColor color = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_TextOnProminentButtonColor, color_scheme);
      return SkColorSetA(color, gfx::kGoogleGreyAlpha700);
    }
    case NativeTheme::kColorId_NotificationColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_TextOnProminentButtonColor, color_scheme);
    case NativeTheme::kColorId_NotificationActionsRowBackground:
    case NativeTheme::kColorId_NotificationBackgroundActive:
    case NativeTheme::kColorId_NotificationLargeImageBackground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_HighlightedMenuItemBackgroundColor,
          color_scheme);
    case NativeTheme::kColorId_MessageCenterSmallImageMaskBackground:
    case NativeTheme::kColorId_NotificationDefaultAccentColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_NotificationInkDropBase:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);

    // Scrollbar
    case NativeTheme::kColorId_OverlayScrollbarThumbFill:
    case NativeTheme::kColorId_OverlayScrollbarThumbHoveredFill: {
      const SkColor fill = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_CustomTabBarForegroundColor, color_scheme);
      const bool hovered =
          color_id == NativeTheme::kColorId_OverlayScrollbarThumbHoveredFill;
      return SkColorSetA(
          fill, hovered ? gfx::kGoogleGreyAlpha800 : gfx::kGoogleGreyAlpha700);
    }
    case NativeTheme::kColorId_OverlayScrollbarThumbStroke:
    case NativeTheme::kColorId_OverlayScrollbarThumbHoveredStroke: {
      const SkColor stroke = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_CustomTabBarBackgroundColor, color_scheme);
      const bool hovered =
          color_id == NativeTheme::kColorId_OverlayScrollbarThumbHoveredStroke;
      return SkColorSetA(stroke, hovered ? gfx::kGoogleGreyAlpha500
                                         : gfx::kGoogleGreyAlpha400);
    }

    // Separator
    case NativeTheme::kColorId_SeparatorColor:
      return gfx::kGoogleGrey300;

    // Shadow
    case NativeTheme::kColorId_ShadowBase:
      return gfx::kGoogleGrey800;

    case NativeTheme::kColorId_ShadowValueAmbientShadowElevationThree:
      return SkColorSetA(base_theme->GetUnprocessedSystemColor(
                             NativeTheme::kColorId_ShadowBase, color_scheme),
                         0x40);
    case NativeTheme::kColorId_ShadowValueKeyShadowElevationThree:
      return SkColorSetA(base_theme->GetUnprocessedSystemColor(
                             NativeTheme::kColorId_ShadowBase, color_scheme),
                         0x66);
    case NativeTheme::kColorId_ShadowValueAmbientShadowElevationSixteen:
      return SkColorSetA(base_theme->GetUnprocessedSystemColor(
                             NativeTheme::kColorId_ShadowBase, color_scheme),
                         0x3d);
    case NativeTheme::kColorId_ShadowValueKeyShadowElevationSixteen:
      return SkColorSetA(base_theme->GetUnprocessedSystemColor(
                             NativeTheme::kColorId_ShadowBase, color_scheme),
                         0x1a);

    // Slider
    case NativeTheme::kColorId_SliderThumbMinimal:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_SliderTroughMinimal:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case NativeTheme::kColorId_SliderThumbDefault:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
    case NativeTheme::kColorId_SliderTroughDefault: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      const SkColor fg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      return color_utils::AlphaBlend(fg, bg, gfx::kGoogleGreyAlpha400);
    }

    // Sync info container
    case NativeTheme::kColorId_SyncInfoContainerNoPrimaryAccount:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_HighlightedMenuItemBackgroundColor,
          color_scheme);
    case NativeTheme::kColorId_SyncInfoContainerPaused: {
      const SkColor fg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      return SkColorSetA(fg, gfx::kGoogleGreyAlpha100);
    }
    case NativeTheme::kColorId_SyncInfoContainerError: {
      const SkColor fg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_AlertSeverityHigh, color_scheme);
      return SkColorSetA(fg, gfx::kGoogleGreyAlpha100);
    }

    // Tabbed pane
    case NativeTheme::kColorId_TabBottomBorder:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case NativeTheme::kColorId_TabTitleColorInactive:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_TabHighlightBackground:
      return SkColorSetA(gfx::kGoogleBlue300, 0x2B);
    case NativeTheme::kColorId_TabHighlightFocusedBackground:
      return SkColorSetA(gfx::kGoogleBlue300, 0x53);
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
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_FocusedBorderColor, color_scheme);
    case NativeTheme::kColorId_TableSelectionBackgroundFocused:
    case NativeTheme::kColorId_TableSelectionBackgroundUnfocused: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      const SkColor fg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      return color_utils::AlphaBlend(fg, bg, SkAlpha{0x3C});
    }
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
    case NativeTheme::kColorId_TextfieldDefaultBackground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_CustomTabBarBackgroundColor, color_scheme);
    case NativeTheme::kColorId_TextfieldPlaceholderColor:
    case NativeTheme::kColorId_TextfieldReadOnlyColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelDisabledColor, color_scheme);
    case NativeTheme::kColorId_TextfieldDefaultColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
    case NativeTheme::kColorId_TextfieldSelectionColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelTextSelectionColor, color_scheme);
    case NativeTheme::kColorId_TextfieldSelectionBackgroundFocused:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelTextSelectionBackgroundFocused,
          color_scheme);

    // Throbber
    case NativeTheme::kColorId_ThrobberWaitingColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SliderTroughDefault, color_scheme);
    case NativeTheme::kColorId_ThrobberSpinningColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);

    // Toggle button
    case NativeTheme::kColorId_ToggleButtonShadowColor:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case ui::NativeTheme::kColorId_ToggleButtonThumbColorOff:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case ui::NativeTheme::kColorId_ToggleButtonThumbColorOn:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
    case ui::NativeTheme::kColorId_ToggleButtonTrackColorOff:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_SeparatorColor, color_scheme);
    case ui::NativeTheme::kColorId_ToggleButtonTrackColorOn:
      return gfx::kGoogleBlue300;

    // Tooltip
    case NativeTheme::kColorId_TooltipBackground: {
      const SkColor bg = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
      return SkColorSetA(bg, 0xCC);
    }
    case NativeTheme::kColorId_TooltipText: {
      const SkColor text = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);
      return SkColorSetA(text, 0xDE);
    }

    // Tooltip icon
    case NativeTheme::kColorId_TooltipIcon:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelSecondaryColor, color_scheme);
    case NativeTheme::kColorId_TooltipIconHovered:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_LabelEnabledColor, color_scheme);

    // Tree
    case NativeTheme::kColorId_TreeBackground:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_WindowBackground, color_scheme);
    case NativeTheme::kColorId_TreeSelectionBackgroundFocused:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_TableSelectionBackgroundFocused, color_scheme);
    case NativeTheme::kColorId_TreeSelectionBackgroundUnfocused:
      return base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_TableSelectionBackgroundUnfocused,
          color_scheme);
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

    // Focus ring
    case NativeTheme::kColorId_FocusAuraColor:
      const SkColor focus_color = base_theme->GetUnprocessedSystemColor(
          NativeTheme::kColorId_ProminentButtonColor, color_scheme);
      return SkColorSetA(focus_color, 0x3D);
  }
}

}  // namespace

SkColor GetAlertSeverityColor(ColorId color_id, bool dark) {
  constexpr auto kColorIdMap =
      base::MakeFixedFlatMap<ColorId, std::array<SkColor, 2>>({
          {kColorAlertHighSeverity, {{gfx::kGoogleRed600, gfx::kGoogleRed300}}},
          {kColorAlertLowSeverity,
           {{gfx::kGoogleGreen700, gfx::kGoogleGreen300}}},
          {kColorAlertMediumSeverity,
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
    absl::optional<SkColor> color =
        GetHighContrastColor(color_id, color_scheme);
    if (color.has_value()) {
      DVLOG(2) << "GetHighContrastColor: "
               << "NativeTheme::ColorId: " << NativeThemeColorIdName(color_id)
               << " Color: " << SkColorName(color.value());
      return color.value();
    }
  }

  if (color_scheme == NativeTheme::ColorScheme::kDark) {
    absl::optional<SkColor> color = GetDarkSchemeColor(color_id, base_theme);
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
