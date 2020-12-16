// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include "ui/base/idle/idle_internal.h"

#if defined(USE_X11)
#include "ui/base/x/x11_idle_query.h"
#include "ui/base/x/x11_screensaver_window_finder.h"
#else
#include "base/notreached.h"
#endif

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#endif

namespace ui {

int CalculateIdleTime() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    auto* const screen = display::Screen::GetScreen();
    // The screen can be nullptr in tests.
    if (!screen)
      return 0;
    return screen->CalculateIdleTime().InSeconds();
  }
#endif
#if defined(USE_X11)
  IdleQueryX11 idle_query;
  return idle_query.IdleTime();
#else
  NOTIMPLEMENTED_LOG_ONCE();
  return 0;
#endif
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value())
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;

#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    auto* const screen = display::Screen::GetScreen();
    // The screen can be nullptr in tests.
    if (!screen)
      return false;
    return screen->IsScreenSaverActive();
  }
#endif
#if defined(USE_X11)
  // Usually the screensaver is used to lock the screen.
  return ScreensaverWindowFinder::ScreensaverWindowExists();
#else
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
#endif
}

}  // namespace ui
