// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <cstring>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_id.h"
#include "ui/color/color_metrics.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_utils.h"

namespace ui {

NativeTheme::MenuListExtraParams::MenuListExtraParams() = default;
NativeTheme::TextFieldExtraParams::TextFieldExtraParams() = default;

NativeTheme::MenuListExtraParams::MenuListExtraParams(
    const NativeTheme::MenuListExtraParams&) = default;

NativeTheme::TextFieldExtraParams::TextFieldExtraParams(
    const NativeTheme::TextFieldExtraParams&) = default;

NativeTheme::MenuListExtraParams& NativeTheme::MenuListExtraParams::operator=(
    const NativeTheme::MenuListExtraParams&) = default;
NativeTheme::TextFieldExtraParams& NativeTheme::TextFieldExtraParams::operator=(
    const NativeTheme::TextFieldExtraParams&) = default;

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_APPLE)
// static
bool NativeTheme::SystemDarkModeSupported() {
  return false;
}
#endif

ColorProviderKey NativeTheme::GetColorProviderKey(
    scoped_refptr<ColorProviderKey::ThemeInitializerSupplier> custom_theme,
    bool use_custom_frame) const {
  return ColorProviderKey(
      (GetDefaultSystemColorScheme() == ColorScheme::kDark)
          ? ColorProviderKey::ColorMode::kDark
          : ColorProviderKey::ColorMode::kLight,
      UserHasContrastPreference() ? ColorProviderKey::ContrastMode::kHigh
                                  : ColorProviderKey::ContrastMode::kNormal,
      system_theme_,
      use_custom_frame ? ui::ColorProviderKey::FrameType::kChromium
                       : ui::ColorProviderKey::FrameType::kNative,
      ui::ColorProviderKey::FrameStyle::kDefault,
      should_use_system_accent_color_
          ? ui::ColorProviderKey::UserColorSource::kAccent
          : ui::ColorProviderKey::UserColorSource::kBaseline,
      user_color_, scheme_variant_, std::move(custom_theme));
}

SkColor NativeTheme::GetSystemButtonPressedColor(SkColor base_color) const {
  return base_color;
}

SkColor4f NativeTheme::FocusRingColorForBaseColor(SkColor4f base_color) const {
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
  base::ElapsedTimer timer;
  auto& color_provider_manager = ui::ColorProviderManager::Get();
  const size_t initial_providers_initialized =
      color_provider_manager.num_providers_initialized();

  // This specific method is prone to being mistakenly called on the wrong
  // sequence, because it is often invoked from a platform-specific event
  // listener, and those events may be delivered on unexpected sequences.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Reset the ColorProviderManager's cache so that ColorProviders requested
  // from this point onwards incorporate the changes to the system theme.
  color_provider_manager.ResetColorProviderCache();
  for (NativeThemeObserver& observer : native_theme_observers_)
    observer.OnNativeThemeUpdated(this);

  RecordNumColorProvidersInitializedDuringOnNativeThemeUpdated(
      color_provider_manager.num_providers_initialized() -
      initial_providers_initialized);
  RecordTimeSpentProcessingOnNativeThemeUpdatedEvent(timer.Elapsed());
}

void NativeTheme::NotifyOnCaptionStyleUpdated() {
  // This specific method is prone to being mistakenly called on the wrong
  // sequence, because it is often invoked from a platform-specific event
  // listener, and those events may be delivered on unexpected sequences.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (NativeThemeObserver& observer : native_theme_observers_)
    observer.OnCaptionStyleUpdated();
}

void NativeTheme::NotifyOnPreferredContrastUpdated() {
  // This specific method is prone to being mistakenly called on the wrong
  // sequence, because it is often invoked from a platform-specific event
  // listener, and those events may be delivered on unexpected sequences.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (NativeThemeObserver& observer : native_theme_observers_)
    observer.OnPreferredContrastChanged();
}

float NativeTheme::AdjustBorderWidthByZoom(float border_width,
                                           float zoom_level) const {
  float zoomed = floorf(border_width * zoom_level);
  return std::max(1.0f, zoomed);
}

float NativeTheme::AdjustBorderRadiusByZoom(Part part,
                                            float border_radius,
                                            float zoom) const {
  if (part == kCheckbox || part == kTextField || part == kPushButton) {
    float zoomed = floorf(border_radius * zoom);
    return std::max(1.0f, zoomed);
  }
  return border_radius;
}

NativeTheme::NativeTheme(bool should_use_dark_colors,
                         ui::SystemTheme system_theme)
    : should_use_dark_colors_(should_use_dark_colors || IsForcedDarkMode()),
      system_theme_(system_theme),
      forced_colors_(IsForcedHighContrast()),
      prefers_reduced_transparency_(false),
      inverted_colors_(false),
      preferred_color_scheme_(CalculatePreferredColorScheme()),
      preferred_contrast_(CalculatePreferredContrast()) {}

NativeTheme::~NativeTheme() = default;

bool NativeTheme::ShouldUseDarkColors() const {
  return should_use_dark_colors_;
}

bool NativeTheme::UserHasContrastPreference() const {
  return GetPreferredContrast() !=
         NativeTheme::PreferredContrast::kNoPreference;
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

NativeTheme::PageColors NativeTheme::GetPageColors() const {
  return page_colors_;
}

NativeTheme::PreferredColorScheme NativeTheme::GetPreferredColorScheme() const {
  return preferred_color_scheme_;
}

bool NativeTheme::GetPrefersReducedTransparency() const {
  return prefers_reduced_transparency_;
}

bool NativeTheme::GetInvertedColors() const {
  return inverted_colors_;
}

NativeTheme::PreferredContrast NativeTheme::GetPreferredContrast() const {
  return preferred_contrast_;
}

void NativeTheme::SetPreferredContrast(
    NativeTheme::PreferredContrast preferred_contrast) {
  if (preferred_contrast_ == preferred_contrast)
    return;
  preferred_contrast_ = preferred_contrast;
  NotifyOnPreferredContrastUpdated();
}

bool NativeTheme::IsForcedDarkMode() {
  static bool kIsForcedDarkMode =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode);
  return kIsForcedDarkMode;
}

bool NativeTheme::IsForcedHighContrast() {
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

absl::optional<CaptionStyle> NativeTheme::GetSystemCaptionStyle() const {
  return CaptionStyle::FromSystemSettings();
}

const std::map<NativeTheme::SystemThemeColor, SkColor>&
NativeTheme::GetSystemColors() const {
  return system_colors_;
}

absl::optional<SkColor> NativeTheme::GetSystemThemeColor(
    SystemThemeColor theme_color) const {
  auto color = system_colors_.find(theme_color);
  if (color != system_colors_.end())
    return color->second;

  return absl::nullopt;
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
  bool prefers_reduced_transparency =
      observed_theme->GetPrefersReducedTransparency();
  PreferredColorScheme preferred_color_scheme =
      observed_theme->GetPreferredColorScheme();
  PreferredContrast preferred_contrast = observed_theme->GetPreferredContrast();
  bool inverted_colors = observed_theme->GetInvertedColors();
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
    theme_to_update_->SetPreferredContrast(preferred_contrast);
    notify_observers = true;
  }
  if (theme_to_update_->GetPrefersReducedTransparency() !=
      prefers_reduced_transparency) {
    theme_to_update_->set_prefers_reduced_transparency(
        prefers_reduced_transparency);
    notify_observers = true;
  }
  if (theme_to_update_->GetInvertedColors() != inverted_colors) {
    theme_to_update_->set_inverted_colors(inverted_colors);
    notify_observers = true;
  }

  const auto& system_colors = observed_theme->GetSystemColors();
  if (theme_to_update_->HasDifferentSystemColors(system_colors)) {
    theme_to_update_->set_system_colors(system_colors);
    notify_observers = true;
  }

  if (notify_observers) {
    DCHECK(theme_to_update_->UserHasContrastPreference() ||
           !theme_to_update_->InForcedColorsMode());
    theme_to_update_->NotifyOnNativeThemeUpdated();
  }
}

NativeTheme::ColorScheme NativeTheme::GetDefaultSystemColorScheme() const {
  return ShouldUseDarkColors() ? ColorScheme::kDark : ColorScheme::kLight;
}

}  // namespace ui
