// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_MOCK_OS_SETTINGS_PROVIDER_H_
#define UI_NATIVE_THEME_MOCK_OS_SETTINGS_PROVIDER_H_

#include <optional>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/os_settings_provider.h"

namespace ui {

// Mock object to allow tests to control OS-level theme settings. Instantiate
// this in your test fixture to have it override the real provider; it will
// automatically notify the NativeTheme of changes made via the setters.
class COMPONENT_EXPORT(NATIVE_THEME) MockOsSettingsProvider
    : public OsSettingsProvider {
 public:
  MockOsSettingsProvider();
  ~MockOsSettingsProvider() override;

  // OsSettingsProvider:
  bool DarkColorSchemeAvailable() const override;
  NativeTheme::PreferredColorScheme PreferredColorScheme() const override;
  ColorProviderKey::UserColorSource PreferredColorSource() const override;
  NativeTheme::PreferredContrast PreferredContrast() const override;
  bool PrefersReducedTransparency() const override;
  bool PrefersInvertedColors() const override;
  bool ForcedColorsActive() const override;
  std::optional<SkColor> AccentColor() const override;
  std::optional<SkColor> Color(ColorId color_id) const override;
  std::optional<ColorProviderKey::SchemeVariant> SchemeVariant() const override;
  base::TimeDelta CaretBlinkInterval() const override;

  // Setters for all the above settings.
  void SetDarkColorSchemeAvailable(bool dark_color_scheme_available);
  void SetPreferredColorScheme(
      NativeTheme::PreferredColorScheme preferred_color_scheme);
  void SetPreferredColorSource(
      ColorProviderKey::UserColorSource preferred_color_source);
  void SetPreferredContrast(NativeTheme::PreferredContrast preferred_contrast);
  void SetPrefersReducedTransparency(bool prefers_reduced_transparency);
  void SetPrefersInvertedColors(bool prefers_inverted_colors);
  void SetForcedColorsActive(bool forced_colors_active);
  void SetAccentColor(SkColor accent_color);
  void SetColor(ColorId color_id, SkColor color);
  void SetSchemeVariant(ColorProviderKey::SchemeVariant scheme_variant);
  void SetCaretBlinkInterval(base::TimeDelta caret_blink_interval);

 private:
  bool dark_color_scheme_available_ = true;
  NativeTheme::PreferredColorScheme preferred_color_scheme_ =
      NativeTheme::PreferredColorScheme::kLight;
  ColorProviderKey::UserColorSource preferred_color_source_ =
      ColorProviderKey::UserColorSource::kBaseline;
  NativeTheme::PreferredContrast preferred_contrast_ =
      NativeTheme::PreferredContrast::kNoPreference;
  bool prefers_reduced_transparency_ = false;
  bool prefers_inverted_colors_ = false;
  bool forced_colors_active_ = false;
  std::optional<SkColor> accent_color_;
  base::flat_map<ColorId, SkColor> colors_;
  std::optional<ColorProviderKey::SchemeVariant> scheme_variant_;
  base::TimeDelta caret_blink_interval_ = kDefaultCaretBlinkInterval;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_MOCK_OS_SETTINGS_PROVIDER_H_
