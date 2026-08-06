// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_QT_OS_SETTINGS_PROVIDER_QT_H_
#define UI_QT_OS_SETTINGS_PROVIDER_QT_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/native_theme/os_settings_provider.h"
#include "ui/qt/qt_interface.h"

namespace qt {

class OsSettingsProviderQt : public ui::OsSettingsProvider {
 public:
  explicit OsSettingsProviderQt(QtInterface* shim);
  OsSettingsProviderQt(const OsSettingsProviderQt&) = delete;
  OsSettingsProviderQt& operator=(const OsSettingsProviderQt&) = delete;
  ~OsSettingsProviderQt() override;

  // ui::OsSettingsProvider:
  ui::NativeTheme::PreferredColorScheme PreferredColorScheme() const override;
  std::optional<SkColor> AccentColor() const override;
  base::TimeDelta CaretBlinkInterval() const override;

  // Sets the system accent color (sourced from the xdg-desktop-portal and
  // pushed in via QtUi::SetAccentColor) and notifies observers.
  void SetAccentColor(std::optional<SkColor> accent_color);

  // Sets the color-scheme preference (sourced from the xdg-desktop-portal and
  // pushed in via QtUi::SetColorScheme) and notifies observers. `std::nullopt`
  // means "no portal preference"; `PreferredColorScheme()` then falls back to
  // the toolkit-derived scheme. Otherwise the value selects dark (true) or
  // light (false).
  void SetColorScheme(std::optional<bool> prefer_dark);

  // Called by QtUi when the Qt theme changes. Unlike GTK, the Qt provider has
  // no settings signals of its own, so QtUi notifies it explicitly to re-derive
  // the toolkit color scheme.
  void OnThemeChanged();

 private:
  // IMPORTANT NOTE: All members that use `shim_` must be decorated with
  // `DISABLE_CFI_VCALL`.
  raw_ptr<QtInterface> shim_;

  std::optional<SkColor> accent_color_;

  // The xdg-desktop-portal color-scheme preference, if any (dark = true,
  // light = false). When unset, `PreferredColorScheme()` derives the scheme
  // from the toolkit theme instead.
  std::optional<bool> prefer_dark_;
};

}  // namespace qt

#endif  // UI_QT_OS_SETTINGS_PROVIDER_QT_H_
