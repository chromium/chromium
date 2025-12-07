// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_ASH_H_
#define UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_ASH_H_

#include <optional>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/os_settings_provider.h"

namespace ui {

class COMPONENT_EXPORT(NATIVE_THEME) OsSettingsProviderAsh
    : public OsSettingsProvider {
 public:
  OsSettingsProviderAsh();
  OsSettingsProviderAsh(const OsSettingsProviderAsh&) = delete;
  OsSettingsProviderAsh& operator=(const OsSettingsProviderAsh&) = delete;
  ~OsSettingsProviderAsh() override;

  // Returns a pointer specifically to the `OsSettingsProviderAsh` instance if
  // it has been created, even if it is not currently the default provider. This
  // is necessary for callers who need to call `SetColorPaletteData()`, since
  // not only does `Get()` not return the right type, but there is no guarantee
  // the returned pointer can be safely downcast -- it may be an unrelated type.
  //
  // If there are currently no providers, this triggers creation of the
  // `OsSettingsProviderAsh` instance, just as `Get()` would. However, if some
  // non-Ash provider was created before any calls to `Get()`, this will not
  // create an Ash instance (as that would override the previously-created
  // provider); it will return null.
  static OsSettingsProviderAsh* GetInstance();

  // OsSettingsProvider:
  NativeTheme::PreferredColorScheme PreferredColorScheme() const override;
  std::optional<SkColor> AccentColor() const override;
  std::optional<ColorProviderKey::SchemeVariant> SchemeVariant() const override;

  // To avoid layering violations, this class does not observe Ash controllers
  // directly; instead they call this method to update its state as necessary.
  // This avoids bringing Ash-specific types or observer interfaces into `//ui`.
  void SetColorPaletteData(
      NativeTheme::PreferredColorScheme preferred_color_scheme,
      std::optional<SkColor> accent_color,
      std::optional<ColorProviderKey::SchemeVariant> scheme_variant);

 private:
  // The `OsSettingsProviderAsh` instance if it has been created, or null.
  static OsSettingsProviderAsh* instance_;

  NativeTheme::PreferredColorScheme preferred_color_scheme_ =
      NativeTheme::PreferredColorScheme::kNoPreference;
  std::optional<SkColor> accent_color_;
  std::optional<ColorProviderKey::SchemeVariant> scheme_variant_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_ASH_H_
