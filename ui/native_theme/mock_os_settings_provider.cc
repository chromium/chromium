// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/mock_os_settings_provider.h"

#include <optional>

#include "base/time/time.h"

namespace ui {

MockOsSettingsProvider::MockOsSettingsProvider()
    : OsSettingsProvider(PriorityLevel::kTesting) {}

MockOsSettingsProvider::~MockOsSettingsProvider() = default;

base::TimeDelta MockOsSettingsProvider::CaretBlinkInterval() const {
  return caret_blink_interval_;
}

void MockOsSettingsProvider::SetCaretBlinkInterval(
    base::TimeDelta caret_blink_interval) {
  caret_blink_interval_ = caret_blink_interval;
  NotifyOnSettingsChanged();
}

}  // namespace ui
