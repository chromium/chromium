// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <cstring>
#include <optional>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_id.h"
#include "ui/color/color_metrics.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_utils.h"

namespace ui {

namespace {
static constexpr base::TimeDelta kDefaultCaretBlinkInterval =
    base::Milliseconds(500);
}

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
  const auto get_forced_colors_key = [](bool forced_colors,
                                        PageColors page_colors) {
    if (!forced_colors) {
      return ColorProviderKey::ForcedColors::kNone;
    }
    static constexpr auto kForcedColorsMap =
        base::MakeFixedFlatMap<PageColors, ColorProviderKey::ForcedColors>(
            {{PageColors::kOff, ColorProviderKey::ForcedColors::kNone},
             {PageColors::kDusk, ColorProviderKey::ForcedColors::kDusk},
             {PageColors::kDesert, ColorProviderKey::ForcedColors::kDesert},
             {PageColors::kNightSky, ColorProviderKey::ForcedColors::kNightSky},
             {PageColors::kWhite, ColorProviderKey::ForcedColors::kWhite},
             {PageColors::kHighContrast,
              ColorProviderKey::ForcedColors::kActive}});

    return kForcedColorsMap.at(page_colors);
  };

  ui::ColorProviderKey key;
  switch (GetDefaultSystemColorScheme()) {
    case ColorScheme::kDark:
      key.color_mode = ColorProviderKey::ColorMode::kDark;
      break;
    case ColorScheme::kLight:
      key.color_mode = ColorProviderKey::ColorMode::kLight;
      break;
    case ColorScheme::kPlatformHighContrast:
      key.color_mode = GetPreferredColorScheme() == PreferredColorScheme::kDark
                           ? ColorProviderKey::ColorMode::kDark
                           : ColorProviderKey::ColorMode::kLight;
      break;
    default:
      NOTREACHED();
  }
  key.contrast_mode = UserHasContrastPreference()
                          ? ColorProviderKey::ContrastMode::kHigh
                          : ColorProviderKey::ContrastMode::kNormal;
  key.forced_colors = get_forced_colors_key(InForcedColorsMode(), page_colors_);
  key.system_theme = system_theme_;
  key.frame_type = use_custom_frame ? ColorProviderKey::FrameType::kChromium
                                    : ColorProviderKey::FrameType::kNative;
  key.user_color_source = should_use_system_accent_color_
                              ? ColorProviderKey::UserColorSource::kAccent
                              : ColorProviderKey::UserColorSource::kBaseline;
  key.user_color = user_color_;
  key.scheme_variant = scheme_variant_;
  key.custom_theme = std::move(custom_theme);

  return key;
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
  native_theme_observers_.Notify(&NativeThemeObserver::OnNativeThemeUpdated,
                                 this);

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
  native_theme_observers_.Notify(&NativeThemeObserver::OnCaptionStyleUpdated);
}

void NativeTheme::NotifyOnPreferredContrastUpdated() {
  // This specific method is prone to being mistakenly called on the wrong
  // sequence, because it is often invoked from a platform-specific event
  // listener, and those events may be delivered on unexpected sequences.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  native_theme_observers_.Notify(
      &NativeThemeObserver::OnPreferredContrastChanged);
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

base::TimeDelta NativeTheme::GetCaretBlinkInterval() const {
  if (caret_blink_interval_.has_value()) {
    return caret_blink_interval_.value();
  }
  std::optional<base::TimeDelta> platform_interval =
      GetPlatformCaretBlinkInterval();
  if (platform_interval.has_value()) {
    return platform_interval.value();
  }
  return kDefaultCaretBlinkInterval;
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

NativeTheme::PreferredColorScheme NativeTheme::CalculatePreferredColorScheme()
    const {
  return ShouldUseDarkColors() ? NativeTheme::PreferredColorScheme::kDark
                               : NativeTheme::PreferredColorScheme::kLight;
}

std::optional<base::TimeDelta> NativeTheme::GetPlatformCaretBlinkInterval()
    const {
  return std::nullopt;
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

NativeTheme::PreferredContrast NativeTheme::CalculatePreferredContrast() const {
  return IsForcedHighContrast() ? PreferredContrast::kMore
                                : PreferredContrast::kNoPreference;
}

std::optional<CaptionStyle> NativeTheme::GetSystemCaptionStyle() const {
  return CaptionStyle::FromSystemSettings();
}

const std::map<NativeTheme::SystemThemeColor, SkColor>&
NativeTheme::GetSystemColors() const {
  return system_colors_;
}

std::optional<SkColor> NativeTheme::GetSystemThemeColor(
    SystemThemeColor theme_color) const {
  auto color = system_colors_.find(theme_color);
  if (color != system_colors_.end())
    return color->second;

  return std::nullopt;
}

bool NativeTheme::HasDifferentSystemColors(
    const std::map<NativeTheme::SystemThemeColor, SkColor>& colors) const {
  return system_colors_ != colors;
}

void NativeTheme::set_system_colors(
    const std::map<NativeTheme::SystemThemeColor, SkColor>& colors) {
  system_colors_ = colors;
}

NativeTheme::ColorSchemeNativeThemeObserver::ColorSchemeNativeThemeObserver(
    NativeTheme* theme_to_update)
    : theme_to_update_(theme_to_update) {}

NativeTheme::ColorSchemeNativeThemeObserver::~ColorSchemeNativeThemeObserver() =
    default;

void NativeTheme::ColorSchemeNativeThemeObserver::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  bool should_use_dark_colors = observed_theme->ShouldUseDarkColors();
  PreferredColorScheme preferred_color_scheme =
      observed_theme->GetPreferredColorScheme();
  bool inverted_colors = observed_theme->GetInvertedColors();
  base::TimeDelta caret_blink_interval =
      observed_theme->GetCaretBlinkInterval();
  bool notify_observers = false;

  if (theme_to_update_->ShouldUseDarkColors() != should_use_dark_colors) {
    theme_to_update_->set_use_dark_colors(should_use_dark_colors);
    notify_observers = true;
  }
  if (theme_to_update_->GetPreferredColorScheme() != preferred_color_scheme) {
    theme_to_update_->set_preferred_color_scheme(preferred_color_scheme);
    notify_observers = true;
  }
  if (theme_to_update_->GetInvertedColors() != inverted_colors) {
    theme_to_update_->set_inverted_colors(inverted_colors);
    notify_observers = true;
  }
  if (theme_to_update_->GetCaretBlinkInterval() != caret_blink_interval) {
    theme_to_update_->set_caret_blink_interval(caret_blink_interval);
    notify_observers = true;
  }

  notify_observers |=
      theme_to_update_->UpdateContrastRelatedStates(*observed_theme);

  if (notify_observers) {
    DCHECK(theme_to_update_->UserHasContrastPreference() ||
           !theme_to_update_->InForcedColorsMode());
    theme_to_update_->NotifyOnNativeThemeUpdated();
  }
}

NativeTheme::ColorScheme NativeTheme::GetDefaultSystemColorScheme() const {
  return ShouldUseDarkColors() ? ColorScheme::kDark : ColorScheme::kLight;
}

bool NativeTheme::UpdateContrastRelatedStates(
    const NativeTheme& observed_theme) {
  bool forced_colors = observed_theme.InForcedColorsMode();
  PageColors page_colors = observed_theme.GetPageColors();
  bool prefers_reduced_transparency =
      observed_theme.GetPrefersReducedTransparency();
  PreferredContrast preferred_contrast = observed_theme.GetPreferredContrast();
  bool states_updated = false;

  const auto default_page_colors =
      forced_colors ? PageColors::kHighContrast : PageColors::kOff;
  if (page_colors != default_page_colors) {
    if (page_colors == PageColors::kOff) {
      forced_colors = false;
      preferred_contrast = PreferredContrast::kNoPreference;
    } else if (page_colors != PageColors::kHighContrast) {
      // Set other states based on the selected theme (i.e. `kDusk`, `kDesert`,
      // `kNightSky`, or `kWhite`). This block is only executed when one of
      // these themes is chosen. `kHighContrast` is not a valid theme here, as
      // it is only available in forced colors mode.
      CHECK_GE(page_colors, ui::NativeTheme::PageColors::kDusk);
      CHECK_LE(page_colors, ui::NativeTheme::PageColors::kWhite);
      forced_colors = true;
      preferred_contrast = PreferredContrast::kMore;
    }
  }

  if (InForcedColorsMode() != forced_colors) {
    set_forced_colors(forced_colors);
    states_updated = true;
  }
  if (GetPageColors() != page_colors) {
    set_page_colors(page_colors);
    // Only update the color scheme if page colors is a selected theme.
    if (page_colors != PageColors::kOff &&
        page_colors != PageColors::kHighContrast) {
      bool is_dark_color = page_colors == PageColors::kNightSky ||
                           page_colors == PageColors::kDusk;
      PreferredColorScheme page_colors_theme_scheme =
          is_dark_color ? PreferredColorScheme::kDark
                        : PreferredColorScheme::kLight;
      set_use_dark_colors(is_dark_color);
      set_preferred_color_scheme(page_colors_theme_scheme);
    }
    states_updated = true;
  }
  if (GetPreferredContrast() != preferred_contrast) {
    SetPreferredContrast(preferred_contrast);
    states_updated = true;
  }
  if (GetPrefersReducedTransparency() != prefers_reduced_transparency) {
    set_prefers_reduced_transparency(prefers_reduced_transparency);
    states_updated = true;
  }

  return states_updated;
}

int NativeTheme::GetPaintedScrollbarTrackInset() const {
  return 0;
}

gfx::Insets NativeTheme::GetScrollbarSolidColorThumbInsets(Part part) const {
  return gfx::Insets();
}

SkColor4f NativeTheme::GetScrollbarThumbColor(
    const ui::ColorProvider& color_provider,
    State state,
    const ScrollbarThumbExtraParams& extra_params) const {
  // A native theme using solid color scrollbar thumb must override this method.
  NOTREACHED();
}

}  // namespace ui
