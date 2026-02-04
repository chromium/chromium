// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/scoped_set_screensaver_state.h"

#include "base/check_is_test.h"
#include "ui/base/idle/screensaver_state_observer.h"

namespace ui {

ScopedSetScreensaverState::ScopedSetScreensaverState(
    bool is_screensaver_running)
    : previous_state_(ScreensaverStateForTesting()) {
  CHECK_IS_TEST();
  ScreensaverStateForTesting() = is_screensaver_running;  // IN-TEST
  ScreensaverStateObserver::GetInstance()->RefreshScreensaverState();
}

ScopedSetScreensaverState::~ScopedSetScreensaverState() {
  ScreensaverStateForTesting() = previous_state_;  // IN-TEST
  ScreensaverStateObserver::GetInstance()->RefreshScreensaverState();
}

}  // namespace ui
