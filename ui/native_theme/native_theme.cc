// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <cstring>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

absl::optional<SkColor> NativeTheme::GetColorProviderColor(
    ColorId color_id,
    ColorScheme color_scheme,
    std::string theme_name) const {
  if (base::FeatureList::IsEnabled(features::kColorProviderRedirection) &&
      AllowColorPipelineRedirection(color_scheme)) {
    if (auto provider_color_id = NativeThemeColorIdToColorId(color_id)) {
      auto* color_provider = ColorProviderManager::Get().GetColorProviderFor(
          {(color_scheme == NativeTheme::ColorScheme::kDark)
               ? ColorProviderManager::ColorMode::kDark
               : ColorProviderManager::ColorMode::kLight,
           (color_scheme == NativeTheme::ColorScheme::kPlatformHighContrast)
               ? ColorProviderManager::ContrastMode::kHigh
               : ColorProviderManager::ContrastMode::kNormal,
           std::move(theme_name)});
      ReportHistogramBooleanUsesColorProvider(true);
      return color_provider->GetColor(provider_color_id.value());
    }
  }
  return absl::nullopt;
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

std::string NativeTheme::GetNativeThemeName() const {
  return std::string();
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

  if (auto color =
          GetColorProviderColor(color_id, color_scheme, GetNativeThemeName())) {
    return color.value();
  }

  ReportHistogramBooleanUsesColorProvider(false);
  return GetSystemColorDeprecated(color_id, color_scheme, apply_processing);
}

}  // namespace ui
