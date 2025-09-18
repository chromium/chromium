// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_WIN_H_
#define UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_WIN_H_

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/win/registry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/win/accent_color_observer.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/native_theme/os_settings_provider.h"

namespace ui {

class COMPONENT_EXPORT(NATIVE_THEME) OsSettingsProviderWin
    : public OsSettingsProvider {
 public:
  OsSettingsProviderWin();
  OsSettingsProviderWin(const OsSettingsProviderWin&) = delete;
  OsSettingsProviderWin& operator=(const OsSettingsProviderWin&) = delete;
  ~OsSettingsProviderWin() override;

  bool DarkColorSchemeAvailable() const override;
  NativeTheme::PreferredColorScheme PreferredColorScheme() const override;
  ColorProviderKey::UserColorSource PreferredColorSource() const override;
  bool PrefersReducedTransparency() const override;
  bool PrefersInvertedColors() const override;
  bool ForcedColorsActive() const override;
  std::optional<SkColor> AccentColor() const override;
  std::optional<SkColor> Color(ColorId color_id) const override;
  base::TimeDelta CaretBlinkInterval() const override;

 private:
  // Registers an observer to monitor the respective registry keys.
  void RegisterThemesRegkeyObserver();
  void RegisterColorFilteringRegkeyObserver();

  // Updates values affected by the respective registry keys.
  void UpdateForThemesRegkey();
  void UpdateForColorFilteringRegkey();

  // Updates `accent_color_`. If it changed, notifies callbacks.
  void OnAccentColorMaybeChanged();

  // Updates the values in `colors_`.
  void UpdateColors();

  // Called by `singleton_hwnd_observer_`.
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Color/high contrast mode change subscription.
  base::CallbackListSubscription hwnd_subscription_ =
      gfx::SingletonHwnd::GetInstance()->RegisterCallback(
          base::BindRepeating(&OsSettingsProviderWin::OnWndProc,
                              base::Unretained(this)));

  // Dark Mode/Transparency registry key.
  base::win::RegKey hkcu_themes_regkey_;

  // Inverted colors registry key.
  base::win::RegKey hkcu_color_filtering_regkey_;

  // Accent color subscription.
  base::CallbackListSubscription accent_color_subscription_ =
      AccentColorObserver::Get()->Subscribe(
          base::BindRepeating(&OsSettingsProviderWin::OnAccentColorMaybeChanged,
                              base::Unretained(this)));

  bool in_dark_mode_ = false;
  bool prefers_reduced_transparency_ = false;
  bool prefers_inverted_colors_ = false;
  bool forced_colors_active_ = false;
  std::optional<SkColor> accent_color_ =
      AccentColorObserver::Get()->accent_color();
  base::flat_map<ColorId, SkColor> colors_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_WIN_H_
