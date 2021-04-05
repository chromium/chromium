// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <cstring>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_utils.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_utils.h"

namespace ui {

namespace {
// clang-format off
base::Optional<ColorId>
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
#if defined(OS_APPLE)
        {NTCID::kColorId_TableBackgroundAlternate,
          kColorTableBackgroundAlternate},
#endif
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
  return base::nullopt;
}
// clang-format on

void ReportHistogramBooleanUsesColorProvider(bool uses_color_provider) {
  UMA_HISTOGRAM_BOOLEAN("NativeTheme.GetSystemColor.UsesColorProvider",
                        uses_color_provider);
}

}  // namespace

NativeTheme::ExtraParams::ExtraParams() {
  memset(this, 0, sizeof(*this));
}

NativeTheme::ExtraParams::ExtraParams(const ExtraParams& other) {
  memcpy(this, &other, sizeof(*this));
}

#if !defined(OS_WIN) && !defined(OS_APPLE)
// static
bool NativeTheme::SystemDarkModeSupported() {
  return false;
}
#endif

SkColor NativeTheme::GetSystemColor(ColorId color_id,
                                    ColorScheme color_scheme) const {
  return GetSystemColorCommon(color_id, color_scheme, true);
}

SkColor NativeTheme::GetUnprocessedSystemColor(ColorId color_id,
                                               ColorScheme color_scheme) const {
  auto color = GetSystemColorCommon(color_id, color_scheme, false);
  DVLOG(2) << "GetUnprocessedSystemColor: "
           << "NativeTheme::ColorId: " << NativeThemeColorIdName(color_id)
           << " Color: " << SkColorName(color);
  return color;
}

SkColor NativeTheme::GetSystemButtonPressedColor(SkColor base_color) const {
  return base_color;
}

SkColor NativeTheme::FocusRingColorForBaseColor(SkColor base_color) const {
  return base_color;
}

float NativeTheme::GetBorderRadiusForPart(Part part,
                                          float width,
                                          float height) const {
  return 0;
}

void NativeTheme::AddObserver(NativeThemeObserver* observer) {
  native_theme_observers_.AddObserver(observer);
}

void NativeTheme::RemoveObserver(NativeThemeObserver* observer) {
  native_theme_observers_.RemoveObserver(observer);
}

void NativeTheme::NotifyOnNativeThemeUpdated() {
  // This specific method is prone to being mistakenly called on the wrong
  // sequence, because it is often invoked from a platform-specific event
  // listener, and those events may be delivered on unexpected sequences.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (NativeThemeObserver& observer : native_theme_observers_)
    observer.OnNativeThemeUpdated(this);
}

void NativeTheme::NotifyOnCaptionStyleUpdated() {
  // This specific method is prone to being mistakenly called on the wrong
  // sequence, because it is often invoked from a platform-specific event
  // listener, and those events may be delivered on unexpected sequences.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (NativeThemeObserver& observer : native_theme_observers_)
    observer.OnCaptionStyleUpdated();
}

NativeTheme::NativeTheme(bool should_use_dark_colors)
    : should_use_dark_colors_(should_use_dark_colors || IsForcedDarkMode()),
      forced_colors_(IsForcedHighContrast()),
      preferred_color_scheme_(CalculatePreferredColorScheme()),
      preferred_contrast_(CalculatePreferredContrast()) {}

NativeTheme::~NativeTheme() = default;

base::Optional<SkColor> NativeTheme::GetColorProviderColor(
    ColorId color_id,
    ColorScheme color_scheme) const {
  if (base::FeatureList::IsEnabled(features::kColorProviderRedirection) &&
      AllowColorPipelineRedirection(color_scheme)) {
    if (auto provider_color_id = NativeThemeColorIdToColorId(color_id)) {
      auto* color_provider = ColorProviderManager::Get().GetColorProviderFor(
          {(color_scheme == NativeTheme::ColorScheme::kDark)
               ? ColorProviderManager::ColorMode::kDark
               : ColorProviderManager::ColorMode::kLight,
           (color_scheme == NativeTheme::ColorScheme::kPlatformHighContrast)
               ? ColorProviderManager::ContrastMode::kHigh
               : ColorProviderManager::ContrastMode::kNormal});
      ReportHistogramBooleanUsesColorProvider(true);
      return color_provider->GetColor(provider_color_id.value());
    }
  }
  return base::nullopt;
}

bool NativeTheme::ShouldUseDarkColors() const {
  return should_use_dark_colors_;
}

bool NativeTheme::UserHasContrastPreference() const {
  return GetPreferredContrast() !=
             NativeTheme::PreferredContrast::kNoPreference ||
         InForcedColorsMode();
}

bool NativeTheme::InForcedColorsMode() const {
  return forced_colors_;
}

NativeTheme::PlatformHighContrastColorScheme
NativeTheme::GetPlatformHighContrastColorScheme() const {
  if (GetDefaultSystemColorScheme() != ColorScheme::kPlatformHighContrast)
    return PlatformHighContrastColorScheme::kNone;
  return (GetPreferredColorScheme() == PreferredColorScheme::kDark)
             ? PlatformHighContrastColorScheme::kDark
             : PlatformHighContrastColorScheme::kLight;
}

NativeTheme::PreferredColorScheme NativeTheme::GetPreferredColorScheme() const {
  return preferred_color_scheme_;
}

NativeTheme::PreferredContrast NativeTheme::GetPreferredContrast() const {
  return preferred_contrast_;
}

bool NativeTheme::IsForcedDarkMode() const {
  static bool kIsForcedDarkMode =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode);
  return kIsForcedDarkMode;
}

bool NativeTheme::IsForcedHighContrast() const {
  static bool kIsForcedHighContrast =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceHighContrast);
  return kIsForcedHighContrast;
}

NativeTheme::PreferredColorScheme NativeTheme::CalculatePreferredColorScheme()
    const {
  return ShouldUseDarkColors() ? NativeTheme::PreferredColorScheme::kDark
                               : NativeTheme::PreferredColorScheme::kLight;
}

NativeTheme::PreferredContrast NativeTheme::CalculatePreferredContrast() const {
  return IsForcedHighContrast() ? PreferredContrast::kMore
                                : PreferredContrast::kNoPreference;
}

bool NativeTheme::AllowColorPipelineRedirection(
    ColorScheme color_scheme) const {
  // TODO(kerenzhu): Don't use UserHasContrastPreference().
  // ColorScheme should encode high contrast info but currently on mac it does
  // not. ColorScheme should also allow combination of light/dark mode with high
  // contrast.
  return color_scheme != ColorScheme::kPlatformHighContrast &&
         !UserHasContrastPreference();
}

SkColor NativeTheme::GetSystemColorDeprecated(ColorId color_id,
                                              ColorScheme color_scheme,
                                              bool apply_processing) const {
  return GetAuraColor(color_id, this, color_scheme);
}

base::Optional<CaptionStyle> NativeTheme::GetSystemCaptionStyle() const {
  return CaptionStyle::FromSystemSettings();
}

const std::map<NativeTheme::SystemThemeColor, SkColor>&
NativeTheme::GetSystemColors() const {
  return system_colors_;
}

base::Optional<SkColor> NativeTheme::GetSystemThemeColor(
    SystemThemeColor theme_color) const {
  auto color = system_colors_.find(theme_color);
  if (color != system_colors_.end())
    return color->second;

  return base::nullopt;
}

bool NativeTheme::HasDifferentSystemColors(
    const std::map<NativeTheme::SystemThemeColor, SkColor>& colors) const {
  return system_colors_ != colors;
}

void NativeTheme::set_system_colors(
    const std::map<NativeTheme::SystemThemeColor, SkColor>& colors) {
  system_colors_ = colors;
}

bool NativeTheme::UpdateSystemColorInfo(
    bool is_dark_mode,
    bool forced_colors,
    const base::flat_map<SystemThemeColor, uint32_t>& colors) {
  bool did_system_color_info_change = false;
  if (is_dark_mode != ShouldUseDarkColors()) {
    did_system_color_info_change = true;
    set_use_dark_colors(is_dark_mode);
  }
  if (forced_colors != InForcedColorsMode()) {
    did_system_color_info_change = true;
    set_forced_colors(forced_colors);
  }
  for (const auto& color : colors) {
    if (color.second != GetSystemThemeColor(color.first)) {
      did_system_color_info_change = true;
      system_colors_[color.first] = color.second;
    }
  }
  return did_system_color_info_change;
}

NativeTheme::ColorSchemeNativeThemeObserver::ColorSchemeNativeThemeObserver(
    NativeTheme* theme_to_update)
    : theme_to_update_(theme_to_update) {}

NativeTheme::ColorSchemeNativeThemeObserver::~ColorSchemeNativeThemeObserver() =
    default;

void NativeTheme::ColorSchemeNativeThemeObserver::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  bool should_use_dark_colors = observed_theme->ShouldUseDarkColors();
  bool forced_colors = observed_theme->InForcedColorsMode();
  PreferredColorScheme preferred_color_scheme =
      observed_theme->GetPreferredColorScheme();
  PreferredContrast preferred_contrast = observed_theme->GetPreferredContrast();
  bool notify_observers = false;

  if (theme_to_update_->ShouldUseDarkColors() != should_use_dark_colors) {
    theme_to_update_->set_use_dark_colors(should_use_dark_colors);
    notify_observers = true;
  }
  if (theme_to_update_->InForcedColorsMode() != forced_colors) {
    theme_to_update_->set_forced_colors(forced_colors);
    notify_observers = true;
  }
  if (theme_to_update_->GetPreferredColorScheme() != preferred_color_scheme) {
    theme_to_update_->set_preferred_color_scheme(preferred_color_scheme);
    notify_observers = true;
  }
  if (theme_to_update_->GetPreferredContrast() != preferred_contrast) {
    theme_to_update_->set_preferred_contrast(preferred_contrast);
    notify_observers = true;
  }

  const auto& system_colors = observed_theme->GetSystemColors();
  if (theme_to_update_->HasDifferentSystemColors(system_colors)) {
    theme_to_update_->set_system_colors(system_colors);
    notify_observers = true;
  }

  if (notify_observers)
    theme_to_update_->NotifyOnNativeThemeUpdated();
}

NativeTheme::ColorScheme NativeTheme::GetDefaultSystemColorScheme() const {
  return ShouldUseDarkColors() ? ColorScheme::kDark : ColorScheme::kLight;
}

SkColor NativeTheme::GetSystemColorCommon(ColorId color_id,
                                          ColorScheme color_scheme,
                                          bool apply_processing) const {
  SCOPED_UMA_HISTOGRAM_TIMER("NativeTheme.GetSystemColor");
  if (color_scheme == NativeTheme::ColorScheme::kDefault)
    color_scheme = GetDefaultSystemColorScheme();

  if (auto color = GetColorProviderColor(color_id, color_scheme))
    return color.value();

  ReportHistogramBooleanUsesColorProvider(false);
  return GetSystemColorDeprecated(color_id, color_scheme, apply_processing);
}

}  // namespace ui
