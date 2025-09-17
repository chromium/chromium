// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/os_settings_provider_ash.h"

#include <optional>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"

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

std::optional<SkColor> OsSettingsProviderAsh::AccentColor() const {
  return accent_color_;
}

std::optional<ColorProviderKey::SchemeVariant>
OsSettingsProviderAsh::SchemeVariant() const {
  return scheme_variant_;
}

void OsSettingsProviderAsh::SetColorPaletteData(
    std::optional<SkColor> accent_color,
    std::optional<ColorProviderKey::SchemeVariant> scheme_variant) {
  accent_color_ = accent_color;
  scheme_variant_ = scheme_variant;
  NotifyOnSettingsChanged();
}

}  // namespace ui
