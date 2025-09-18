// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_MAC_H_
#define UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_MAC_H_

#include <memory>

#include "base/component_export.h"
#include "ui/native_theme/os_settings_provider.h"

namespace ui {

class COMPONENT_EXPORT(NATIVE_THEME) OsSettingsProviderMac
    : public OsSettingsProvider {
 public:
  OsSettingsProviderMac();
  OsSettingsProviderMac(const OsSettingsProviderMac&) = delete;
  OsSettingsProviderMac& operator=(const OsSettingsProviderMac&) = delete;
  ~OsSettingsProviderMac() override;

  // OsSettingsProvider:
  NativeTheme::PreferredColorScheme PreferredColorScheme() const override;
  NativeTheme::PreferredContrast PreferredContrast() const override;
  bool PrefersReducedTransparency() const override;
  bool PrefersInvertedColors() const override;
  base::TimeDelta CaretBlinkInterval() const override;

 private:
  // Because this header is #included from C++ source, we can't use Obj-C here.
  // Instead the Obj-C members are defined entirely in the .mm.
  struct ObjCMembers;

  std::unique_ptr<ObjCMembers> objc_members_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_MAC_H_
