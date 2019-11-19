// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include <limits.h>
#include <windows.h>

#include "ui/base/idle/idle_internal.h"
#include "ui/base/win/lock_state.h"

namespace ui {
namespace {

DWORD CalculateIdleTimeInternal() {
  LASTINPUTINFO last_input_info = {0};
  last_input_info.cbSize = sizeof(LASTINPUTINFO);
  DWORD current_idle_time = 0;
  if (::GetLastInputInfo(&last_input_info)) {
    DWORD now = ::GetTickCount();
    if (now < last_input_info.dwTime) {
      // GetTickCount() wraps around every 49.7 days -- assume it wrapped just
      // once.
      const DWORD kMaxDWORD = static_cast<DWORD>(-1);
      DWORD time_before_wrap = kMaxDWORD - last_input_info.dwTime;
      DWORD time_after_wrap = now;
      // The sum is always smaller than kMaxDWORD.
      current_idle_time = time_before_wrap + time_after_wrap;
    } else {
      current_idle_time = now - last_input_info.dwTime;
    }

    // Convert from ms to seconds.
    current_idle_time /= 1000;
  }

  return current_idle_time;
}

bool IsScreensaverRunning() {
  DWORD result = 0;
  if (::SystemParametersInfo(SPI_GETSCREENSAVERRUNNING, 0, &result, 0))
    return result != FALSE;
  return false;
}

}  // namespace

int CalculateIdleTime() {
  return static_cast<int>(CalculateIdleTimeInternal());
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value())
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;

  return ui::IsWorkstationLocked() || IsScreensaverRunning();
}

}  // namespace ui
