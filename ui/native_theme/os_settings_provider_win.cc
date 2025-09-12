// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/os_settings_provider_win.h"

#include <windows.h>

#include "base/time/time.h"

namespace ui {

OsSettingsProviderWin::OsSettingsProviderWin()
    : OsSettingsProvider(PriorityLevel::kProduction) {}

OsSettingsProviderWin::~OsSettingsProviderWin() = default;

base::TimeDelta OsSettingsProviderWin::CaretBlinkInterval() const {
  // Unfortunately Windows does not seem to have any way to monitor changes to
  // this value; MSDN suggests apps "occasionally check the cursor settings —
  // for instance, when the dialog is loaded"
  // (https://learn.microsoft.com/en-us/previous-versions/windows/desktop/dnacc/flashing-user-interface-and-the-getcaretblinktime-function#using-getcaretblinktime).
  // Given how rarely users change this, it doesn't seem worth trying to plumb
  // something to e.g. check for caret blink time changes when Chrome regains
  // focus.
  const UINT caret_blink_time = ::GetCaretBlinkTime();
  if (!caret_blink_time) {
    return OsSettingsProvider::CaretBlinkInterval();
  }
  return (caret_blink_time == INFINITE) ? base::TimeDelta()
                                        : base::Milliseconds(caret_blink_time);
}

}  // namespace ui
