// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_ANDROID_H_
#define UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_ANDROID_H_

#include "base/component_export.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/accessibility/android/accessibility_state.h"
#include "ui/native_theme/os_settings_provider.h"

namespace ui {

class COMPONENT_EXPORT(NATIVE_THEME) OsSettingsProviderAndroid
    : public OsSettingsProvider,
      public AccessibilityState::AccessibilityStateObserver {
 public:
  OsSettingsProviderAndroid();
  OsSettingsProviderAndroid(const OsSettingsProviderAndroid&) = delete;
  OsSettingsProviderAndroid& operator=(const OsSettingsProviderAndroid&) =
      delete;
  ~OsSettingsProviderAndroid() override;

  // OsSettingsProvider:
  NativeTheme::PreferredContrast PreferredContrast() const override;
  bool PrefersReducedTransparency() const override;
  bool PrefersInvertedColors() const override;
  base::TimeDelta CaretBlinkInterval() const override;

  // AccessibilityState::AccessibilityStateObserver:
  void OnDisplayInversionEnabledChanged(bool enabled) override;
  void OnContrastLevelChanged(bool high_contrast_enabled) override;
  void OnTextCursorBlinkIntervalChanged(base::TimeDelta new_interval) override;

 private:
  base::ScopedObservation<AccessibilityState,
                          AccessibilityState::AccessibilityStateObserver>
      accessibility_state_observation_{this};

  bool high_contrast_enabled_ = false;
  bool display_inversion_enabled_ = false;
  base::TimeDelta text_cursor_blink_interval_ = kDefaultCaretBlinkInterval;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_ANDROID_H_
