// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/screensaver_state_observer.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace ui {

namespace {

std::optional<bool>& GetScreensaverStateForTestingStorage() {
  static std::optional<bool> state;
  return state;
}

// Queries the system for the current screensaver running state.
// In tests, if ScreensaverStateForTesting() has been set, returns that value
// instead of calling the actual Windows API.
bool QueryScreensaverRunningFromSystem() {
  std::optional<bool>& test_state = GetScreensaverStateForTestingStorage();
  if (test_state.has_value()) {
    return test_state.value();
  }

  DWORD result = 0;
  if (::SystemParametersInfo(SPI_GETSCREENSAVERRUNNING, 0, &result, 0)) {
    return result != FALSE;
  }
  return false;
}

}  // namespace

std::optional<bool>& ScreensaverStateForTesting() {
  CHECK_IS_TEST();
  return GetScreensaverStateForTestingStorage();
}

// static
ScreensaverStateObserver* ScreensaverStateObserver::GetInstance() {
  // The singleton must be created on the UI thread (required for SingletonHwnd
  // registration). C++11 guarantees thread-safe initialization of statics.
  if (!base::CurrentUIThread::IsSet()) {
    return nullptr;
  }
  static base::NoDestructor<ScreensaverStateObserver> instance;
  return instance.get();
}

ScreensaverStateObserver::ScreensaverStateObserver() {
  // Get initial state.
  is_screensaver_running_.store(QueryScreensaverRunningFromSystem(),
                                std::memory_order_relaxed);

  // Register callback to receive WM_SETTINGCHANGE messages.
  hwnd_subscription_ =
      gfx::SingletonHwnd::GetInstance()->RegisterCallback(base::BindRepeating(
          &ScreensaverStateObserver::OnWndProc, base::Unretained(this)));
}

ScreensaverStateObserver::~ScreensaverStateObserver() = default;

bool ScreensaverStateObserver::IsScreensaverRunning() const {
  return is_screensaver_running_.load(std::memory_order_relaxed);
}

void ScreensaverStateObserver::RefreshScreensaverState() {
  UpdateScreensaverState();
}

void ScreensaverStateObserver::OnWndProc(HWND hwnd,
                                         UINT message,
                                         WPARAM wparam,
                                         LPARAM lparam) {
  if (message != WM_SETTINGCHANGE) {
    return;
  }

  // SPI_SETSCREENSAVEACTIVE is sent when screensaver enabled/disabled changes.
  // SPI_SETSCREENSAVERRUNNING is sent when screensaver starts/stops.
  // These messages are rare, so calling SystemParametersInfo inline is fine.
  if (wparam == SPI_SETSCREENSAVEACTIVE ||
      wparam == SPI_SETSCREENSAVERRUNNING) {
    UpdateScreensaverState();
  }
}

void ScreensaverStateObserver::UpdateScreensaverState() {
  is_screensaver_running_.store(QueryScreensaverRunningFromSystem(),
                                std::memory_order_relaxed);
}

}  // namespace ui
