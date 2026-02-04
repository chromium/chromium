// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include <windows.h>

#include <limits.h>
#include <powrprof.h>

#include "base/callback_list.h"
#include "base/check_is_test.h"
#include "base/no_destructor.h"
#include "ui/base/idle/idle_internal.h"
#include "ui/base/idle/screensaver_state_observer.h"
#include "ui/base/win/lock_state.h"
#include "ui/base/win/session_change_observer.h"

namespace ui {

namespace {

base::RepeatingCallbackList<void(bool)>& GetScreenLockCallbacks();

void OnSessionChange(WPARAM wparam, const bool* is_current_session) {
  if (wparam != WTS_SESSION_LOCK && wparam != WTS_SESSION_UNLOCK) {
    return;
  }

  bool locked = wparam == WTS_SESSION_LOCK;
  GetScreenLockCallbacks().Notify(locked);
}

// Keep a SessionChangeObserver around, creating it on first use.
SessionChangeObserver* GetSessionChangeObserver() {
  static base::NoDestructor<SessionChangeObserver> observer(
      base::BindRepeating(&OnSessionChange));
  return observer.get();
}

base::RepeatingCallbackList<void(bool)>& GetScreenLockCallbacks() {
  static base::NoDestructor<base::RepeatingCallbackList<void(bool)>> callbacks;
  return *callbacks;
}

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
  // Use the cached value from the ScreensaverStateObserver if available.
  // This avoids the slow SystemParametersInfo call on the UI thread.
  // The observer handles test overrides via ScreensaverStateForTesting().
  ScreensaverStateObserver* observer = ScreensaverStateObserver::GetInstance();
  if (observer) {
    return observer->IsScreensaverRunning();
  }

  // There should be no production codepaths that reach here, but it is possible
  // for some test scenarios to do so.
  CHECK_IS_TEST();
  return false;
}

}  // namespace

base::CallbackListSubscription AddScreenLockCallback(
    base::RepeatingCallback<void(bool)> callback) {
  if (GetScreenLockCallbacks().empty()) {
    GetSessionChangeObserver();
  }
  return GetScreenLockCallbacks().Add(std::move(callback));
}

int CalculateIdleTime() {
  return static_cast<int>(CalculateIdleTimeInternal());
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value()) {
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;
  }

  return ui::IsWorkstationLocked() || IsScreensaverRunning();
}

}  // namespace ui
