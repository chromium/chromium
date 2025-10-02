// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/os_settings_provider_android.h"

#include <utility>

#include "base/scoped_observation.h"
#include "ui/accessibility/android/accessibility_state.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

OsSettingsProviderAndroid::OsSettingsProviderAndroid()
    : OsSettingsProvider(PriorityLevel::kProduction) {
  accessibility_state_observation_.Observe(AccessibilityState::Get());
}

OsSettingsProviderAndroid::~OsSettingsProviderAndroid() = default;

NativeTheme::PreferredContrast OsSettingsProviderAndroid::PreferredContrast()
    const {
  return high_contrast_enabled_ ? NativeTheme::PreferredContrast::kMore
                                : NativeTheme::PreferredContrast::kNoPreference;
}

bool OsSettingsProviderAndroid::PrefersReducedTransparency() const {
  return high_contrast_enabled_;
}

bool OsSettingsProviderAndroid::PrefersInvertedColors() const {
  return display_inversion_enabled_;
}

base::TimeDelta OsSettingsProviderAndroid::CaretBlinkInterval() const {
  return text_cursor_blink_interval_;
}

void OsSettingsProviderAndroid::OnDisplayInversionEnabledChanged(bool enabled) {
  if (std::exchange(display_inversion_enabled_, enabled) != enabled) {
    NotifyOnSettingsChanged();
  }
}

void OsSettingsProviderAndroid::OnContrastLevelChanged(
    bool high_contrast_enabled) {
  if (std::exchange(high_contrast_enabled_, high_contrast_enabled) !=
      high_contrast_enabled) {
    NotifyOnSettingsChanged();
  }
}

void OsSettingsProviderAndroid::OnTextCursorBlinkIntervalChanged(
    base::TimeDelta new_interval) {
  if (std::exchange(text_cursor_blink_interval_, new_interval) !=
      new_interval) {
    NotifyOnSettingsChanged();
  }
}

}  // namespace ui
