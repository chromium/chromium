// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/os_settings_provider_ash.h"

#include <optional>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

// static
OsSettingsProviderAsh* OsSettingsProviderAsh::instance_ = nullptr;

OsSettingsProviderAsh::OsSettingsProviderAsh()
    : OsSettingsProvider(PriorityLevel::kProduction) {
  // Nesting multiple `OsSettingsProviderAsh` instances is not currently
  // supported.
  CHECK(!instance_);
  instance_ = this;
}

OsSettingsProviderAsh::~OsSettingsProviderAsh() {
  CHECK_EQ(instance_, this);
  instance_ = nullptr;
}

// static
OsSettingsProviderAsh* OsSettingsProviderAsh::GetInstance() {
  Get();
  return instance_;
}

NativeTheme::PreferredColorScheme OsSettingsProviderAsh::PreferredColorScheme()
    const {
  return preferred_color_scheme_;
}

std::optional<SkColor> OsSettingsProviderAsh::AccentColor() const {
  return accent_color_;
}

std::optional<ColorProviderKey::SchemeVariant>
OsSettingsProviderAsh::SchemeVariant() const {
  return scheme_variant_;
}

void OsSettingsProviderAsh::SetColorPaletteData(
    NativeTheme::PreferredColorScheme preferred_color_scheme,
    std::optional<SkColor> accent_color,
    std::optional<ColorProviderKey::SchemeVariant> scheme_variant) {
  preferred_color_scheme_ = preferred_color_scheme;
  accent_color_ = accent_color;
  scheme_variant_ = scheme_variant;
  NotifyOnSettingsChanged();
}

}  // namespace ui
