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

}  // namespace qt
