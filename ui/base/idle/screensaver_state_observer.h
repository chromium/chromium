// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IDLE_SCREENSAVER_STATE_OBSERVER_H_
#define UI_BASE_IDLE_SCREENSAVER_STATE_OBSERVER_H_

#include <windows.h>

#include <atomic>
#include <optional>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/no_destructor.h"

namespace ui {

// An optional screensaver state set by tests to override the actual
// screensaver state of the system.
COMPONENT_EXPORT(UI_BASE_IDLE)
std::optional<bool>& ScreensaverStateForTesting();

// Observes WM_SETTINGCHANGE messages to track screensaver state changes
// and caches the result to avoid slow SystemParametersInfo calls on the
// UI thread.
class COMPONENT_EXPORT(UI_BASE_IDLE) ScreensaverStateObserver {
 public:
  // Returns the singleton instance. May return nullptr if not initialized
  // (e.g., if there's no UI thread message loop).
  static ScreensaverStateObserver* GetInstance();

  ScreensaverStateObserver(const ScreensaverStateObserver&) = delete;
  ScreensaverStateObserver& operator=(const ScreensaverStateObserver&) = delete;

  // Returns the cached screensaver running state.
  bool IsScreensaverRunning() const;

  // Forces a refresh of the cached screensaver state from the system.
  // This is primarily for testing purposes.
  void RefreshScreensaverState();

 private:
  friend class base::NoDestructor<ScreensaverStateObserver>;

  ScreensaverStateObserver();
  ~ScreensaverStateObserver();

  // Called when a WM_SETTINGCHANGE message is received.
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Queries the system for the current screensaver state and updates the
  // cached value.
  void UpdateScreensaverState();

  // The cached screensaver running state. Using atomic for thread-safety
  // since this may be queried from different threads.
  std::atomic<bool> is_screensaver_running_{false};

  // Subscription to receive window messages from SingletonHwnd.
  std::optional<base::CallbackListSubscription> hwnd_subscription_;
};

}  // namespace ui

#endif  // UI_BASE_IDLE_SCREENSAVER_STATE_OBSERVER_H_
