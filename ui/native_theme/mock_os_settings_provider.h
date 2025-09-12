// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_MOCK_OS_SETTINGS_PROVIDER_H_
#define UI_NATIVE_THEME_MOCK_OS_SETTINGS_PROVIDER_H_

#include <optional>

#include "base/component_export.h"
#include "base/time/time.h"
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
  base::TimeDelta CaretBlinkInterval() const override;

  // Setters for all the above settings.
  void SetCaretBlinkInterval(base::TimeDelta caret_blink_interval);

 private:
  base::TimeDelta caret_blink_interval_ = kDefaultCaretBlinkInterval;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_MOCK_OS_SETTINGS_PROVIDER_H_
