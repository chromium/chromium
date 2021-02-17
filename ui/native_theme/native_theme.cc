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
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/native_theme/common_theme.h"

#if !defined(OS_ANDROID)
#include "ui/color/color_mixers.h"
#endif

namespace ui {

namespace {
// clang-format off
bool NativeThemeColorIdToColorId(NativeTheme::ColorId native_theme_color_id,
                                 ColorId* color_id) {
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
        {NTCID::kColorId_BubbleFooterBackground,
          kColorBubbleFooterBackground},
        {NTCID::kColorId_ButtonColor, kColorButtonBackground},
        {NTCID::kColorId_ButtonBorderColor, kColorButtonBorder},
        {NTCID::kColorId_DisabledButtonBorderColor, kColorButtonBorderDisabled},
        {NTCID::kColorId_ButtonDisabledColor,
          kColorButtonForegroundDisabled},
        {NTCID::kColorId_ButtonEnabledColor, kColorButtonForeground},
        {NTCID::kColorId_ProminentButtonColor,
          kColorButtonBackgroundProminent},
        {NTCID::kColorId_ProminentButtonDisabledColor,
          kColorButtonBackgroundProminentDisabled},
        {NTCID::kColorId_ProminentButtonFocusedColor,
          kColorButtonBackgroundProminentFocused},
        {NTCID::kColorId_TextOnProminentButtonColor,
          kColorButtonForegroundProminent},
        {NTCID::kColorId_ButtonUncheckedColor,
          kColorButtonForegroundUnchecked},
        {NTCID::kColorId_DialogBackground, kColorDialogBackground},
        {NTCID::kColorId_DialogForeground, kColorDialogForeground},
        {NTCID::kColorId_FocusedBorderColor, kColorFocusableBorderFocused},
        {NTCID::kColorId_UnfocusedBorderColor,
          kColorFocusableBorderUnfocused},
        {NTCID::kColorId_MenuIconColor, kColorMenuIcon},
        {NTCID::kColorId_DefaultIconColor, kColorIcon},
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
        {NTCID::kColorId_MenuItemInitialAlertBackgroundColor,
          kColorMenuItemBackgroundAlertedInitial},
        {NTCID::kColorId_MenuItemTargetAlertBackgroundColor,
          kColorMenuItemBackgroundAlertedTarget},
        {NTCID::kColorId_DisabledMenuItemForegroundColor,
          kColorMenuItemForegroundDisabled},
        {NTCID::kColorId_EnabledMenuItemForegroundColor,
          kColorMenuItemForeground},
        {NTCID::kColorId_HighlightedMenuItemBackgroundColor,
          kColorMenuItemBackgroundHighlighted},
        {NTCID::kColorId_HighlightedMenuItemForegroundColor,
          kColorMenuItemForegroundHighlighted},
        {NTCID::kColorId_MenuItemMinorTextColor,
          kColorMenuItemForegroundSecondary},
        {NTCID::kColorId_FocusedMenuItemBackgroundColor,
          kColorMenuItemBackgroundSelected},
        {NTCID::kColorId_SelectedMenuItemForegroundColor,
          kColorMenuItemForegroundSelected},
        {NTCID::kColorId_MenuSeparatorColor, kColorMenuSeparator},
        {NTCID::kColorId_TabBottomBorder, kColorTabContentSeparator},
        {NTCID::kColorId_TabTitleColorInactive, kColorTabForeground},
        {NTCID::kColorId_TabSelectedBorderColor, kColorTabBorderSelected},
        {NTCID::kColorId_TabTitleColorActive, kColorTabForegroundSelected},
        {NTCID::kColorId_TableBackground, kColorTableBackground},
#if defined(OS_APPLE)
        {NTCID::kColorId_TableBackgroundAlternate,
          kColorTableBackgroundAlternate},
#endif
        {NTCID::kColorId_TableText, kColorTableForeground},
        {NTCID::kColorId_TableGroupingIndicatorColor,
          kColorTableGroupingIndicator},
        {NTCID::kColorId_TableHeaderBackground,
          kColorTableHeaderBackground},
        {NTCID::kColorId_TableHeaderText, kColorTableHeaderForeground},
        // TODO(http://crbug.com/1057754): kColorId_TableHeaderSeparator,
        // which is implemented as a native theme override on Mac.
        {NTCID::kColorId_TableSelectionBackgroundFocused,
          kColorTableBackgroundSelectedFocused},
        {NTCID::kColorId_TableSelectedText,
          kColorTableForegroundSelectedFocused},
        {NTCID::kColorId_TableSelectionBackgroundUnfocused,
          kColorTableBackgroundSelectedUnfocused},
        {NTCID::kColorId_TableSelectedTextUnfocused,
          kColorTableForegroundSelectedUnfocused},
        {NTCID::kColorId_TextfieldDefaultBackground,
          kColorTextfieldBackground},
        {NTCID::kColorId_TextfieldReadOnlyBackground,
          kColorTextfieldBackgroundDisabled},
        {NTCID::kColorId_TextfieldReadOnlyColor,
          kColorTextfieldForegroundDisabled},
        {NTCID::kColorId_TextfieldPlaceholderColor,
          kColorTextfieldForegroundPlaceholder},
        {NTCID::kColorId_TextfieldDefaultColor, kColorTextfieldForeground},
        {NTCID::kColorId_TextfieldSelectionBackgroundFocused,
          kColorTextfieldSelectionBackground},
        {NTCID::kColorId_TextfieldSelectionColor,
          kColorTextfieldSelectionForeground},
        {NTCID::kColorId_ThrobberSpinningColor, kColorThrobber},
        {NTCID::kColorId_TooltipBackground, kColorTooltipBackground},
        {NTCID::kColorId_TooltipText, kColorTooltipForeground},
        {NTCID::kColorId_TreeBackground, kColorTreeBackground},
        {NTCID::kColorId_TreeText, kColorTreeNodeForeground},
        {NTCID::kColorId_TreeSelectionBackgroundFocused,
          kColorTreeNodeBackgroundSelectedFocused},
        {NTCID::kColorId_TreeSelectedText,
          kColorTreeNodeForegroundSelectedFocused},
        {NTCID::kColorId_TreeSelectionBackgroundUnfocused,
          kColorTreeNodeBackgroundSelectedUnfocused},
        {NTCID::kColorId_TreeSelectedTextUnfocused,
          kColorTreeNodeForegroundSelectedUnfocused},
        {NTCID::kColorId_WindowBackground, kColorWindowBackground},
      });
  DCHECK(color_id);
  auto* color_it = map.find(native_theme_color_id);
  if (color_it != map.cend()) {
    *color_id = color_it->second;
    return true;
  }
  return false;
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
  SCOPED_UMA_HISTOGRAM_TIMER("NativeTheme.GetSystemColor");
  if (color_scheme == NativeTheme::ColorScheme::kDefault)
    color_scheme = GetDefaultSystemColorScheme();

  // TODO(http://crbug.com/1057754): Remove the below restrictions.
  if (base::FeatureList::IsEnabled(features::kColorProviderRedirection) &&
      color_scheme != NativeTheme::ColorScheme::kPlatformHighContrast) {
    auto color_mode = (color_scheme == NativeTheme::ColorScheme::kDark)
                          ? ColorProviderManager::ColorMode::kDark
                          : ColorProviderManager::ColorMode::kLight;
    // TODO(http://crbug.com/1057754): Handle high contrast modes.
    auto* color_provider = ColorProviderManager::Get().GetColorProviderFor(
        color_mode, ColorProviderManager::ContrastMode::kNormal);
    ui::ColorId provider_color_id;
    if (NativeThemeColorIdToColorId(color_id, &provider_color_id)) {
      ReportHistogramBooleanUsesColorProvider(true);
      return color_provider->GetColor(provider_color_id);
    }
  }
  ReportHistogramBooleanUsesColorProvider(false);
  return GetAuraColor(color_id, this, color_scheme);
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

void NativeTheme::NotifyObservers() {
  for (NativeThemeObserver& observer : native_theme_observers_)
    observer.OnNativeThemeUpdated(this);
}

NativeTheme::NativeTheme(bool should_use_dark_colors)
    : should_use_dark_colors_(should_use_dark_colors || IsForcedDarkMode()),
      forced_colors_(IsForcedHighContrast()),
      preferred_color_scheme_(CalculatePreferredColorScheme()),
      preferred_contrast_(CalculatePreferredContrast()) {
#if !defined(OS_ANDROID)
  // TODO(http://crbug.com/1057754): Merge this into the ColorProviderManager.
  static base::OnceClosure color_provider_manager_init = base::BindOnce([]() {
    ColorProviderManager::Get().SetColorProviderInitializer(base::BindRepeating(
        [](ColorProvider* provider, ColorProviderManager::ColorMode color_mode,
           ColorProviderManager::ContrastMode contrast_mode) {
          const bool is_dark_color_mode =
              color_mode == ColorProviderManager::ColorMode::kDark;
          ui::AddCoreDefaultColorMixer(provider, is_dark_color_mode);
          ui::AddNativeCoreColorMixer(provider, is_dark_color_mode);
          ui::AddUiColorMixer(provider);
          ui::AddNativeUiColorMixer(provider, is_dark_color_mode);
        }));
  });
  if (!color_provider_manager_init.is_null())
    std::move(color_provider_manager_init).Run();
#endif  // !defined(OS_ANDROID)
}

NativeTheme::~NativeTheme() = default;

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
    theme_to_update_->NotifyObservers();
}

NativeTheme::ColorScheme NativeTheme::GetDefaultSystemColorScheme() const {
  return ShouldUseDarkColors() ? ColorScheme::kDark : ColorScheme::kLight;
}

}  // namespace ui
