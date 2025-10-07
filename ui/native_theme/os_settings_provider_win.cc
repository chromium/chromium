// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/os_settings_provider_win.h"

#include <windows.h>

#include <array>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/win/dark_mode_support.h"
#include "base/win/registry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/win/accent_color_observer.h"
#include "ui/color/win/native_color_mixers_win.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

namespace {

bool IsSystemForcedColorsActive() {
  if (HIGHCONTRAST result = {.cbSize = sizeof(HIGHCONTRAST)};
      SystemParametersInfo(SPI_GETHIGHCONTRAST, result.cbSize, &result, 0)) {
    return !!(result.dwFlags & HCF_HIGHCONTRASTON);
  }
  return false;
}

}  // namespace

OsSettingsProviderWin::OsSettingsProviderWin()
    : OsSettingsProvider(PriorityLevel::kProduction) {
  // If there's no sequenced task runner handle, we can't be called back for
  // registry changes. This generally happens in tests.
  const bool observers_can_operate =
      base::SequencedTaskRunner::HasCurrentDefault();

  // Set initial state, and register for future changes if applicable.
  if (hkcu_themes_regkey_.Open(
          HKEY_CURRENT_USER,
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
          KEY_READ | KEY_NOTIFY) == ERROR_SUCCESS) {
    UpdateForThemesRegkey();
    if (observers_can_operate) {
      RegisterThemesRegkeyObserver();
    }
  }
  if (hkcu_color_filtering_regkey_.Open(
          HKEY_CURRENT_USER, L"Software\\Microsoft\\ColorFiltering",
          KEY_READ | KEY_NOTIFY) == ERROR_SUCCESS) {
    UpdateForColorFilteringRegkey();
    if (observers_can_operate) {
      RegisterColorFilteringRegkeyObserver();
    }
  }
  UpdateColors();

  // Initialize forced colors (high contrast) state.
  forced_colors_active_ = IsSystemForcedColorsActive();

  // Histogram high contrast state.
  // NOTE: Reported in metrics; do not reorder, add additional values at end.
  enum class HighContrastColorScheme {
    kNone = 0,
    kDark = 1,
    kLight = 2,
    kMaxValue = kLight,
  };
  auto color_scheme = HighContrastColorScheme::kNone;
  if (PreferredContrast() == NativeTheme::PreferredContrast::kMore) {
    color_scheme =
        (PreferredColorScheme() == NativeTheme::PreferredColorScheme::kDark)
            ? HighContrastColorScheme::kDark
            : HighContrastColorScheme::kLight;
  }
  base::UmaHistogramEnumeration("Accessibility.WinHighContrastTheme",
                                color_scheme);
}

OsSettingsProviderWin::~OsSettingsProviderWin() = default;

bool OsSettingsProviderWin::DarkColorSchemeAvailable() const {
  return base::win::IsDarkModeAvailable();
}

NativeTheme::PreferredColorScheme OsSettingsProviderWin::PreferredColorScheme()
    const {
  if (const NativeTheme::PreferredColorScheme preferred_color_scheme =
          OsSettingsProvider::PreferredColorScheme();
      preferred_color_scheme !=
      NativeTheme::PreferredColorScheme::kNoPreference) {
    return preferred_color_scheme;
  }

  return in_dark_mode_ ? NativeTheme::PreferredColorScheme::kDark
                       : NativeTheme::PreferredColorScheme::kLight;
}

ColorProviderKey::UserColorSource OsSettingsProviderWin::PreferredColorSource()
    const {
  return ColorProviderKey::UserColorSource::kBaseline;
}

bool OsSettingsProviderWin::PrefersReducedTransparency() const {
  return prefers_reduced_transparency_;
}

bool OsSettingsProviderWin::PrefersInvertedColors() const {
  return prefers_inverted_colors_;
}

bool OsSettingsProviderWin::ForcedColorsActive() const {
  return forced_colors_active_;
}

std::optional<SkColor> OsSettingsProviderWin::AccentColor() const {
  return accent_color_;
}

std::optional<SkColor> OsSettingsProviderWin::Color(ColorId color_id) const {
  const auto entry = colors_.find(color_id);
  return (entry == colors_.end()) ? std::nullopt
                                  : std::make_optional(entry->second);
}

base::TimeDelta OsSettingsProviderWin::CaretBlinkInterval() const {
  // Unfortunately Windows does not seem to have any way to monitor changes to
  // this value; MSDN suggests apps "occasionally check the cursor settings —
  // for instance, when the dialog is loaded"
  // (https://learn.microsoft.com/en-us/previous-versions/windows/desktop/dnacc/flashing-user-interface-and-the-getcaretblinktime-function#using-getcaretblinktime).
  // Given how rarely users change this, it doesn't seem worth trying to plumb
  // something to e.g. check for caret blink time changes when Chrome regains
  // focus.
  const UINT caret_blink_time = ::GetCaretBlinkTime();
  if (!caret_blink_time) {
    return OsSettingsProvider::CaretBlinkInterval();
  }
  return (caret_blink_time == INFINITE) ? base::TimeDelta()
                                        : base::Milliseconds(caret_blink_time);
}

void OsSettingsProviderWin::RegisterThemesRegkeyObserver() {
  CHECK(hkcu_themes_regkey_.Valid());
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  hkcu_themes_regkey_.StartWatching(base::BindOnce(
      [](OsSettingsProviderWin* provider) {
        const NativeTheme::PreferredColorScheme old_preferred_color_scheme =
            provider->PreferredColorScheme();
        const bool old_prefers_reduced_transparency =
            provider->PrefersReducedTransparency();
        provider->UpdateForThemesRegkey();
        if (provider->PreferredColorScheme() != old_preferred_color_scheme ||
            provider->PrefersReducedTransparency() !=
                old_prefers_reduced_transparency) {
          provider->NotifyOnSettingsChanged();
        }

        // `StartWatching()`'s callback is one-shot and must be re-registered
        // for future notifications.
        provider->RegisterThemesRegkeyObserver();
      },
      base::Unretained(this)));
}

void OsSettingsProviderWin::RegisterColorFilteringRegkeyObserver() {
  CHECK(hkcu_color_filtering_regkey_.Valid());
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  hkcu_color_filtering_regkey_.StartWatching(base::BindOnce(
      [](OsSettingsProviderWin* provider) {
        const bool old_prefers_inverted_colors =
            provider->PrefersInvertedColors();
        provider->UpdateForColorFilteringRegkey();
        if (provider->PrefersInvertedColors() != old_prefers_inverted_colors) {
          provider->NotifyOnSettingsChanged();
        }

        // `StartWatching()`'s callback is one-shot and must be re-registered
        // for future notifications.
        provider->RegisterColorFilteringRegkeyObserver();
      },
      base::Unretained(this)));
}

void OsSettingsProviderWin::UpdateForThemesRegkey() {
  CHECK(hkcu_themes_regkey_.Valid());

  DWORD apps_use_light_theme = 1;
  hkcu_themes_regkey_.ReadValueDW(L"AppsUseLightTheme", &apps_use_light_theme);
  in_dark_mode_ = !apps_use_light_theme;

  DWORD enable_transparency = 1;
  hkcu_themes_regkey_.ReadValueDW(L"EnableTransparency", &enable_transparency);
  prefers_reduced_transparency_ = !enable_transparency;
}

void OsSettingsProviderWin::UpdateForColorFilteringRegkey() {
  CHECK(hkcu_color_filtering_regkey_.Valid());

  DWORD active = 0, filter_type = 0;
  hkcu_color_filtering_regkey_.ReadValueDW(L"Active", &active);
  if (active == 1) {
    hkcu_color_filtering_regkey_.ReadValueDW(L"FilterType", &filter_type);
  }
  // 0 = Greyscale
  // 1 = Invert
  // 2 = Greyscale Inverted
  // 3 = Deuteranopia
  // 4 = Protanopia
  // 5 = Tritanopia
  prefers_inverted_colors_ = filter_type == 1;
}

void OsSettingsProviderWin::OnAccentColorMaybeChanged() {
  const auto accent_color = AccentColorObserver::Get()->accent_color();
  if (std::exchange(accent_color_, accent_color) != accent_color) {
    NotifyOnSettingsChanged();
  }
}

void OsSettingsProviderWin::UpdateColors() {
  static constexpr auto kColors =
      std::to_array<std::pair<ColorId, ui::ColorId>>(
          {{ColorId::kButtonFace, kColorNativeBtnFace},
           {ColorId::kButtonHighlight, kColorNativeBtnHighlight},
           {ColorId::kScrollbar, kColorNativeScrollbar},
           {ColorId::kWindow, kColorNativeWindow},
           {ColorId::kWindowText, kColorNativeWindowText}});
  const auto sys_colors = GetCurrentSysColors();
  for (const auto& entry : kColors) {
    colors_[entry.first] = sys_colors.at(entry.second);
  }
}

void OsSettingsProviderWin::OnWndProc(HWND hwnd,
                                      UINT message,
                                      WPARAM wparam,
                                      LPARAM lparam) {
  if (message == WM_SYSCOLORCHANGE) {
    UpdateColors();
    if (ForcedColorsActive()) {
      NotifyOnSettingsChanged(true);
    }
  } else if (message == WM_SETTINGCHANGE && wparam == SPI_SETHIGHCONTRAST) {
    const bool old_forced_colors_active = ForcedColorsActive();
    forced_colors_active_ = IsSystemForcedColorsActive();
    if (ForcedColorsActive() != old_forced_colors_active) {
      NotifyOnSettingsChanged();
    }
  }
}

}  // namespace ui
