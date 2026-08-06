// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/qt/os_settings_provider_qt.h"

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/qt/qt_interface.h"

namespace qt {

OsSettingsProviderQt::OsSettingsProviderQt(QtInterface* shim)
    : OsSettingsProvider(PriorityLevel::kProduction), shim_(shim) {}

OsSettingsProviderQt::~OsSettingsProviderQt() = default;

DISABLE_CFI_VCALL
ui::NativeTheme::PreferredColorScheme
OsSettingsProviderQt::PreferredColorScheme() const {
  // The xdg-desktop-portal color-scheme preference (pushed in via
  // QtUi::SetColorScheme) takes precedence when it expresses one.
  if (prefer_dark_) {
    return *prefer_dark_ ? ui::NativeTheme::PreferredColorScheme::kDark
                         : ui::NativeTheme::PreferredColorScheme::kLight;
  }
  return color_utils::IsDark(
             shim_->GetColor(ColorType::kWindowBg, ColorState::kNormal))
             ? ui::NativeTheme::PreferredColorScheme::kDark
             : ui::NativeTheme::PreferredColorScheme::kLight;
}

DISABLE_CFI_VCALL
base::TimeDelta OsSettingsProviderQt::CaretBlinkInterval() const {
  // Unfortunately Qt does not seem to have any way to monitor changes to this
  // value; the docs "recommend that widgets do not cache this value". Chrome is
  // not built to constantly recheck the value, so for now we'll just ignore
  // changes while running. (Windows has the same problem.)
  return base::Milliseconds(shim_->GetCursorBlinkIntervalMs());
}

std::optional<SkColor> OsSettingsProviderQt::AccentColor() const {
  return accent_color_;
}

void OsSettingsProviderQt::SetAccentColor(std::optional<SkColor> accent_color) {
  if (accent_color_ == accent_color) {
    return;
  }
  accent_color_ = accent_color;
  NotifyOnSettingsChanged();
}

void OsSettingsProviderQt::SetColorScheme(std::optional<bool> prefer_dark) {
  if (prefer_dark_ == prefer_dark) {
    return;
  }
  prefer_dark_ = prefer_dark;
  NotifyOnSettingsChanged();
}

void OsSettingsProviderQt::OnThemeChanged() {
  NotifyOnSettingsChanged();
}

}  // namespace qt
