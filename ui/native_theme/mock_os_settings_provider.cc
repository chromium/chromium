// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/mock_os_settings_provider.h"

#include <optional>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {

MockOsSettingsProvider::MockOsSettingsProvider()
    : OsSettingsProvider(PriorityLevel::kTesting) {}

MockOsSettingsProvider::~MockOsSettingsProvider() = default;

bool MockOsSettingsProvider::DarkColorSchemeAvailable() const {
  return dark_color_scheme_available_;
}

bool MockOsSettingsProvider::PrefersReducedTransparency() const {
  return prefers_reduced_transparency_;
}

bool MockOsSettingsProvider::PrefersInvertedColors() const {
  return prefers_inverted_colors_;
}

std::optional<SkColor> MockOsSettingsProvider::Color(ColorId color_id) const {
  const auto it = colors_.find(color_id);
  return (it == colors_.end()) ? std::nullopt : std::make_optional(it->second);
}

base::TimeDelta MockOsSettingsProvider::CaretBlinkInterval() const {
  return caret_blink_interval_;
}

void MockOsSettingsProvider::SetDarkColorSchemeAvailable(
    bool dark_color_scheme_available) {
  dark_color_scheme_available_ = dark_color_scheme_available;
  NotifyOnSettingsChanged();
}

void MockOsSettingsProvider::SetPrefersReducedTransparency(
    bool prefers_reduced_transparency) {
  prefers_reduced_transparency_ = prefers_reduced_transparency;
  NotifyOnSettingsChanged();
}

void MockOsSettingsProvider::SetPrefersInvertedColors(
    bool prefers_inverted_colors) {
  prefers_inverted_colors_ = prefers_inverted_colors;
  NotifyOnSettingsChanged();
}

void MockOsSettingsProvider::SetColor(ColorId color_id, SkColor color) {
  colors_[color_id] = color;
  NotifyOnSettingsChanged();
}

void MockOsSettingsProvider::SetCaretBlinkInterval(
    base::TimeDelta caret_blink_interval) {
  caret_blink_interval_ = caret_blink_interval;
  NotifyOnSettingsChanged();
}

}  // namespace ui
