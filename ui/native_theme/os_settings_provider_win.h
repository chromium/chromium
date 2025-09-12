// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_WIN_H_
#define UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_WIN_H_

#include "base/component_export.h"
#include "ui/native_theme/os_settings_provider.h"

namespace ui {

class COMPONENT_EXPORT(NATIVE_THEME) OsSettingsProviderWin
    : public OsSettingsProvider {
 public:
  OsSettingsProviderWin();
  OsSettingsProviderWin(const OsSettingsProviderWin&) = delete;
  OsSettingsProviderWin& operator=(const OsSettingsProviderWin&) = delete;
  ~OsSettingsProviderWin() override;

  base::TimeDelta CaretBlinkInterval() const override;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_OS_SETTINGS_PROVIDER_WIN_H_
