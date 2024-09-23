// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/lock_state.h"

#include <windows.h>

#include <wtsapi32.h>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "ui/base/win/session_change_observer.h"

namespace ui {

namespace {

// Checks if the current session is locked.
bool IsSessionLocked() {
  bool is_locked = false;
  LPWSTR buffer = nullptr;
  DWORD buffer_length = 0;
  if (::WTSQuerySessionInformation(WTS_CURRENT_SERVER, WTS_CURRENT_SESSION,
                                   WTSSessionInfoEx, &buffer, &buffer_length) &&
      buffer_length >= sizeof(WTSINFOEXW)) {
    auto* info = reinterpret_cast<WTSINFOEXW*>(buffer);
    is_locked =
        info->Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_LOCK;
  }
  if (buffer)
    ::WTSFreeMemory(buffer);
  return is_locked;
}

// Observes the screen lock state of Windows and caches the current state. This
// is necessary as IsSessionLocked uses WTSQuerySessionInformation internally,
// which is an expensive syscall and causes a performance regression as we query
// the current state quite often. http://crbug.com/940607.
class SessionLockedObserver {
 public:
  SessionLockedObserver()
      : session_change_observer_(
            base::BindRepeating(&SessionLockedObserver::OnSessionChange,
                                base::Unretained(this))),
        screen_locked_(IsSessionLocked()) {}

  SessionLockedObserver(const SessionLockedObserver&) = delete;
  SessionLockedObserver& operator=(const SessionLockedObserver&) = delete;

  bool IsLocked() const { return screen_locked_; }

 private:
  void OnSessionChange(WPARAM status_code, const bool* is_current_session) {
    if (is_current_session && !*is_current_session)
      return;
    if (status_code == WTS_SESSION_UNLOCK)
      screen_locked_ = false;
    else if (status_code == WTS_SESSION_LOCK && is_current_session)
      screen_locked_ = true;
  }
  SessionChangeObserver session_change_observer_;
  bool screen_locked_;
};

}  // namespace

bool IsWorkstationLocked() {
  static const base::NoDestructor<SessionLockedObserver> observer;
  return observer->IsLocked();
}

}  // namespace ui
