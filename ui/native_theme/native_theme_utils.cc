// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_utils.h"

#include "base/containers/fixed_flat_map.h"
#include "ui/color/color_id.h"
#include "ui/native_theme/native_theme_color_id.h"

namespace ui {

base::StringPiece NativeThemeColorIdName(NativeTheme::ColorId color_id) {
  static constexpr const auto color_id_names =
      base::MakeFixedFlatMap<NativeTheme::ColorId, const char*>({
#define OP(enum_name) {NativeTheme::ColorId::enum_name, #enum_name}
          NATIVE_THEME_COLOR_IDS
#undef OP
      });
  auto* it = color_id_names.find(color_id);
  DCHECK_NE(color_id_names.cend(), it);
  return it->second;
}

base::StringPiece NativeThemeColorSchemeName(
    NativeTheme::ColorScheme color_scheme) {
  switch (color_scheme) {
    case NativeTheme::ColorScheme::kDefault:
      return "kDefault";
    case NativeTheme::ColorScheme::kLight:
      return "kLight";
    case NativeTheme::ColorScheme::kDark:
      return "kDark";
    case NativeTheme::ColorScheme::kPlatformHighContrast:
      return "kPlatformHighContrast";
    default:
      NOTREACHED() << "Invalid NativeTheme::ColorScheme";
      return "<invalid>";
  }
}

// clang-format off
absl::optional<ColorId>
NativeThemeColorIdToColorId(NativeTheme::ColorId native_theme_color_id) {
  using NTCID = NativeTheme::ColorId;
  static constexpr const auto map =
      base::MakeFixedFlatMap<NativeTheme::ColorId, ColorId>({
        {NTCID::kColorId_AlertSeverityHigh, kColorAlertHighSeverity},
        {NTCID::kColorId_AlertSeverityLow, kColorAlertLowSeverity},
        {NTCID::kColorId_AlertSeverityMedium, kColorAlertMediumSeverity},
        {NTCID::kColorId_AvatarHeaderArt, kColorAvatarHeaderArt},
        {NTCID::kColorId_AvatarIconGuest, kColorAvatarIconGuest},
        {NTCID::kColorId_AvatarIconIncognito, kColorAvatarIconIncognito},
        {NTCID::kColorId_BubbleBackground, kColorBubbleBackground},
        {NTCID::kColorId_BubbleBorder, kColorBubbleBorder},
        {NTCID::kColorId_BubbleBorderShadowLarge,
          kColorBubbleBorderShadowLarge},
        {NTCID::kColorId_BubbleBorderShadowSmall,
          kColorBubbleBorderShadowSmall},
        {NTCID::kColorId_BubbleBorderWhenShadowPresent,
          kColorBubbleBorderWhenShadowPresent},
        {NTCID::kColorId_BubbleFooterBackground,
          kColorBubbleFooterBackground},
        {NTCID::kColorId_ButtonBorderColor, kColorButtonBorder},
        {NTCID::kColorId_ButtonCheckedColor, kColorButtonForegroundChecked},
        {NTCID::kColorId_ButtonColor, kColorButtonBackground},
        {NTCID::kColorId_ButtonDisabledColor,
          kColorButtonForegroundDisabled},
        {NTCID::kColorId_ButtonEnabledColor, kColorButtonForeground},
        {NTCID::kColorId_ButtonUncheckedColor,
          kColorButtonForegroundUnchecked},
        {NTCID::kColorId_CustomFrameActiveColor, kColorFrameActive},
        {NTCID::kColorId_CustomFrameInactiveColor, kColorFrameInactive},
        {NTCID::kColorId_CustomTabBarBackgroundColor,
          kColorPwaToolbarBackground},
        {NTCID::kColorId_CustomTabBarForegroundColor,
          kColorPwaToolbarForeground},
        {NTCID::kColorId_CustomTabBarSecurityChipDangerousColor,
          kColorPwaSecurityChipForegroundDangerous},
        {NTCID::kColorId_CustomTabBarSecurityChipDefaultColor,
          kColorPwaSecurityChipForeground},
        {NTCID::kColorId_CustomTabBarSecurityChipSecureColor,
          kColorPwaSecurityChipForegroundSecure},
        {NTCID::kColorId_CustomTabBarSecurityChipWithCertColor,
          kColorPwaSecurityChipForegroundPolicyCert},
        {NTCID::kColorId_DefaultIconColor, kColorIcon},
        {NTCID::kColorId_DialogBackground, kColorDialogBackground},
        {NTCID::kColorId_DialogForeground, kColorDialogForeground},
        {NTCID::kColorId_DisabledButtonBorderColor, kColorButtonBorderDisabled},
        {NTCID::kColorId_DisabledIconColor, kColorIconDisabled},
        {NTCID::kColorId_SecondaryIconColor, kColorIconSecondary},
        {NTCID::kColorId_DisabledMenuItemForegroundColor,
          kColorMenuItemForegroundDisabled},
        {NTCID::kColorId_DropdownBackgroundColor, kColorDropdownBackground},
        {NTCID::kColorId_DropdownForegroundColor, kColorDropdownForeground},
        {NTCID::kColorId_DropdownSelectedBackgroundColor,
          kColorDropdownBackgroundSelected},
        {NTCID::kColorId_DropdownSelectedForegroundColor,
          kColorDropdownForegroundSelected},
        {NTCID::kColorId_EnabledMenuItemForegroundColor,
          kColorMenuItemForeground},
        {NTCID::kColorId_FocusedBorderColor, kColorFocusableBorderFocused},
        {NTCID::kColorId_FocusedMenuItemBackgroundColor,
          kColorMenuItemBackgroundSelected},
        {NTCID::kColorId_FootnoteContainerBorder, kColorBubbleFooterBorder},
        {NTCID::kColorId_HighlightedMenuItemBackgroundColor,
          kColorMenuItemBackgroundHighlighted},
        {NTCID::kColorId_HighlightedMenuItemForegroundColor,
          kColorMenuItemForegroundHighlighted},
        {NTCID::kColorId_LabelDisabledColor, kColorLabelForegroundDisabled},
        {NTCID::kColorId_LabelEnabledColor, kColorLabelForeground},
        {NTCID::kColorId_LabelSecondaryColor,
          kColorLabelForegroundSecondary},
        {NTCID::kColorId_LabelTextSelectionBackgroundFocused,
          kColorLabelSelectionBackground},
        {NTCID::kColorId_LabelTextSelectionColor,
          kColorLabelSelectionForeground},
        {NTCID::kColorId_LinkDisabled, kColorLinkForegroundDisabled},
        {NTCID::kColorId_LinkEnabled, kColorLinkForeground},
        {NTCID::kColorId_LinkPressed, kColorLinkForegroundPressed},
        {NTCID::kColorId_MenuBackgroundColor, kColorMenuBackground},
        {NTCID::kColorId_MenuBorderColor, kColorMenuBorder},
        {NTCID::kColorId_MenuDropIndicator, kColorMenuDropmarker},
        {NTCID::kColorId_MenuIconColor, kColorMenuIcon},
        {NTCID::kColorId_MenuItemInitialAlertBackgroundColor,
          kColorMenuItemBackgroundAlertedInitial},
        {NTCID::kColorId_MenuItemMinorTextColor,
          kColorMenuItemForegroundSecondary},
        {NTCID::kColorId_MenuItemTargetAlertBackgroundColor,
          kColorMenuItemBackgroundAlertedTarget},
        {NTCID::kColorId_MenuSeparatorColor, kColorMenuSeparator},
        {NTCID::kColorId_MessageCenterSmallImageMaskBackground,
          kColorNotificationIconBackground},
        {NTCID::kColorId_MessageCenterSmallImageMaskForeground,
          kColorNotificationIconForeground},
        {NTCID::kColorId_NotificationActionsRowBackground,
          kColorNotificationActionsBackground},
        {NTCID::kColorId_NotificationBackground,
          kColorNotificationBackgroundInactive},
        {NTCID::kColorId_NotificationBackgroundActive,
          kColorNotificationBackgroundActive},
        {NTCID::kColorId_NotificationColor, kColorNotificationInputForeground},
        {NTCID::kColorId_NotificationDefaultAccentColor,
          kColorNotificationHeaderForeground},
        {NTCID::kColorId_NotificationInkDropBase,
          kColorNotificationInputBackground},
        {NTCID::kColorId_NotificationLargeImageBackground,
          kColorNotificationImageBackground},
        {NTCID::kColorId_NotificationPlaceholderColor,
          kColorNotificationInputPlaceholderForeground},
        {NTCID::kColorId_OverlayScrollbarThumbFill, kColorOverlayScrollbarFill},
        {NTCID::kColorId_OverlayScrollbarThumbHoveredFill,
          kColorOverlayScrollbarFillHovered},
        {NTCID::kColorId_OverlayScrollbarThumbHoveredStroke,
          kColorOverlayScrollbarStrokeHovered},
        {NTCID::kColorId_OverlayScrollbarThumbStroke,
          kColorOverlayScrollbarStroke},
        {NTCID::kColorId_ProminentButtonColor,
          kColorButtonBackgroundProminent},
        {NTCID::kColorId_ProminentButtonDisabledColor,
          kColorButtonBackgroundProminentDisabled},
        {NTCID::kColorId_ProminentButtonFocusedColor,
          kColorButtonBackgroundProminentFocused},
        {NTCID::kColorId_SelectedMenuItemForegroundColor,
          kColorMenuItemForegroundSelected},
        {NTCID::kColorId_SeparatorColor, kColorSeparator},
        {NTCID::kColorId_ShadowBase, kColorShadowBase},
        {NTCID::kColorId_ShadowValueAmbientShadowElevationThree,
          kColorShadowValueAmbientShadowElevationThree},
        {NTCID::kColorId_ShadowValueKeyShadowElevationThree,
          kColorShadowValueKeyShadowElevationThree},
        {NTCID::kColorId_ShadowValueAmbientShadowElevationSixteen,
          kColorShadowValueAmbientShadowElevationSixteen},
        {NTCID::kColorId_ShadowValueKeyShadowElevationSixteen,
          kColorShadowValueKeyShadowElevationSixteen},
        {NTCID::kColorId_SliderThumbDefault, kColorSliderThumb},
        {NTCID::kColorId_SliderThumbMinimal, kColorSliderThumbMinimal},
        {NTCID::kColorId_SliderTroughDefault, kColorSliderTrack},
        {NTCID::kColorId_SliderTroughMinimal, kColorSliderTrackMinimal},
        {NTCID::kColorId_SyncInfoContainerError, kColorSyncInfoBackgroundError},
        {NTCID::kColorId_SyncInfoContainerNoPrimaryAccount,
          kColorSyncInfoBackground},
        {NTCID::kColorId_SyncInfoContainerPaused,
          kColorSyncInfoBackgroundPaused},
        {NTCID::kColorId_TabBottomBorder, kColorTabContentSeparator},
        {NTCID::kColorId_TabHighlightBackground,
          kColorTabBackgroundHighlighted},
        {NTCID::kColorId_TabHighlightFocusedBackground,
          kColorTabBackgroundHighlightedFocused},
        {NTCID::kColorId_TableBackground, kColorTableBackground},
        {NTCID::kColorId_TableBackgroundAlternate,
          kColorTableBackgroundAlternate},
        {NTCID::kColorId_TableGroupingIndicatorColor,
          kColorTableGroupingIndicator},
        {NTCID::kColorId_TableHeaderBackground, kColorTableHeaderBackground},
        {NTCID::kColorId_TableHeaderSeparator, kColorTableHeaderSeparator},
        {NTCID::kColorId_TableHeaderText, kColorTableHeaderForeground},
        {NTCID::kColorId_TableSelectedText,
          kColorTableForegroundSelectedFocused},
        {NTCID::kColorId_TableSelectedTextUnfocused,
          kColorTableForegroundSelectedUnfocused},
        {NTCID::kColorId_TableSelectionBackgroundFocused,
          kColorTableBackgroundSelectedFocused},
        {NTCID::kColorId_TableSelectionBackgroundUnfocused,
          kColorTableBackgroundSelectedUnfocused},
        {NTCID::kColorId_TableText, kColorTableForeground},
        {NTCID::kColorId_TabSelectedBorderColor, kColorTabBorderSelected},
        {NTCID::kColorId_TabTitleColorActive, kColorTabForegroundSelected},
        {NTCID::kColorId_TabTitleColorInactive, kColorTabForeground},
        {NTCID::kColorId_TextfieldDefaultBackground,
          kColorTextfieldBackground},
        {NTCID::kColorId_TextfieldDefaultColor, kColorTextfieldForeground},
        {NTCID::kColorId_TextfieldPlaceholderColor,
          kColorTextfieldForegroundPlaceholder},
        {NTCID::kColorId_TextfieldReadOnlyBackground,
          kColorTextfieldBackgroundDisabled},
        {NTCID::kColorId_TextfieldReadOnlyColor,
          kColorTextfieldForegroundDisabled},
        {NTCID::kColorId_TextfieldSelectionBackgroundFocused,
          kColorTextfieldSelectionBackground},
        {NTCID::kColorId_TextfieldSelectionColor,
          kColorTextfieldSelectionForeground},
        {NTCID::kColorId_TextOnProminentButtonColor,
          kColorButtonForegroundProminent},
        {NTCID::kColorId_ThrobberSpinningColor, kColorThrobber},
        {NTCID::kColorId_ThrobberWaitingColor, kColorThrobberPreconnect},
        {NTCID::kColorId_ToggleButtonShadowColor, kColorToggleButtonShadow},
        {NTCID::kColorId_ToggleButtonThumbColorOff, kColorToggleButtonThumbOff},
        {NTCID::kColorId_ToggleButtonThumbColorOn, kColorToggleButtonThumbOn},
        {NTCID::kColorId_ToggleButtonTrackColorOff, kColorToggleButtonTrackOff},
        {NTCID::kColorId_ToggleButtonTrackColorOn, kColorToggleButtonTrackOn},
        {NTCID::kColorId_TooltipBackground, kColorTooltipBackground},
        {NTCID::kColorId_TooltipIcon, kColorHelpIconInactive},
        {NTCID::kColorId_TooltipIconHovered, kColorHelpIconActive},
        {NTCID::kColorId_TooltipText, kColorTooltipForeground},
        {NTCID::kColorId_TreeBackground, kColorTreeBackground},
        {NTCID::kColorId_TreeSelectedText,
          kColorTreeNodeForegroundSelectedFocused},
        {NTCID::kColorId_TreeSelectedTextUnfocused,
          kColorTreeNodeForegroundSelectedUnfocused},
        {NTCID::kColorId_TreeSelectionBackgroundFocused,
          kColorTreeNodeBackgroundSelectedFocused},
        {NTCID::kColorId_TreeSelectionBackgroundUnfocused,
          kColorTreeNodeBackgroundSelectedUnfocused},
        {NTCID::kColorId_TreeText, kColorTreeNodeForeground},
        {NTCID::kColorId_UnfocusedBorderColor, kColorFocusableBorderUnfocused},
        {NTCID::kColorId_WindowBackground, kColorWindowBackground},
      });
  auto* color_it = map.find(native_theme_color_id);
  if (color_it != map.cend()) {
    return color_it->second;
  }
  return absl::nullopt;
}
// clang-format on

}  // namespace ui
