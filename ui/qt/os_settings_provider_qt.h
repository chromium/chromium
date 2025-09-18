// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_QT_OS_SETTINGS_PROVIDER_QT_H_
#define UI_QT_OS_SETTINGS_PROVIDER_QT_H_

#include "base/memory/raw_ptr.h"
#include "ui/native_theme/os_settings_provider.h"
#include "ui/qt/qt_interface.h"

namespace qt {

class OsSettingsProviderQt : public ui::OsSettingsProvider {
 public:
  explicit OsSettingsProviderQt(QtInterface* shim);
  OsSettingsProviderQt(const OsSettingsProviderQt&) = delete;
  OsSettingsProviderQt& operator=(const OsSettingsProviderQt&) = delete;
  ~OsSettingsProviderQt() override;

  // ui::OsSettingsProvider:
  ui::NativeTheme::PreferredColorScheme PreferredColorScheme() const override;
  base::TimeDelta CaretBlinkInterval() const override;

 private:
  // IMPORTANT NOTE: All members that use `shim_` must be decorated with
  // `DISABLE_CFI_VCALL`.
  raw_ptr<QtInterface> shim_;
};

}  // namespace qt

#endif  // UI_QT_OS_SETTINGS_PROVIDER_QT_H_
