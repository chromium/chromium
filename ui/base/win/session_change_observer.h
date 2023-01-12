// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_SESSION_CHANGE_OBSERVER_H_
#define UI_BASE_WIN_SESSION_CHANGE_OBSERVER_H_

#include <windows.h>

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace ui {

// Calls the provided callback on WM_WTSSESSION_CHANGE messages along with
// managing the tricky business of observing a singleton object. Only
// WTS_SESSION_LOCK and WTS_SESSION_UNLOCK events trigger the callback
// because those are the only events existing observers handle.
class COMPONENT_EXPORT(UI_BASE) SessionChangeObserver {
 public:
  // WPARAM is the wparam passed to the OnWndProc when message is
  // WM_WTSSESSION_CHANGE. The bool indicates whether the session
  // change is for the current session or not. If we couldn't get the current
  // session id, it will be nullptr.
  using WtsCallback = base::RepeatingCallback<void(WPARAM, const bool*)>;
  explicit SessionChangeObserver(const WtsCallback& callback);

  SessionChangeObserver(const SessionChangeObserver&) = delete;
  SessionChangeObserver& operator=(const SessionChangeObserver&) = delete;

  ~SessionChangeObserver();

 private:
  class WtsRegistrationNotificationManager;

  void OnSessionChange(WPARAM wparam, const bool* is_current_session);
  void ClearCallback();

  WtsCallback callback_;
};

}  // namespace ui

#endif  // UI_BASE_WIN_SESSION_CHANGE_OBSERVER_H_
