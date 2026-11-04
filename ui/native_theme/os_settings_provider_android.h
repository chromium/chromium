// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_ANDROID_H_
#define UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_ANDROID_H_

#include <optional>

#include "base/component_export.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
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
  NativeTheme::PreferredColorScheme PreferredColorScheme() const override;
  NativeTheme::PreferredContrast PreferredContrast() const override;
  bool PrefersReducedTransparency() const override;
  bool PrefersInvertedColors() const override;
  std::optional<SkColor> AccentColor() const override;
  base::TimeDelta CaretBlinkInterval() const override;
  bool IsAndroidProvider() const override;

  // AccessibilityState::AccessibilityStateObserver:
  void OnDisplayInversionEnabledChanged(bool enabled) override;
  void OnContrastLevelChanged(bool high_contrast_enabled) override;
  void OnTextCursorBlinkIntervalChanged(base::TimeDelta new_interval) override;

  // Android JNI APIs:
  void SetPreferredColorScheme(NativeTheme::PreferredColorScheme scheme);
  void SetAccentColor(std::optional<SkColor> accent_color);

 private:
  base::ScopedObservation<AccessibilityState,
                          AccessibilityState::AccessibilityStateObserver>
      accessibility_state_observation_{this};

  bool high_contrast_enabled_ = false;
  bool display_inversion_enabled_ = false;
  base::TimeDelta text_cursor_blink_interval_ = kDefaultCaretBlinkInterval;

  NativeTheme::PreferredColorScheme preferred_color_scheme_ =
      NativeTheme::PreferredColorScheme::kNoPreference;
  std::optional<SkColor> accent_color_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_ANDROID_H_
