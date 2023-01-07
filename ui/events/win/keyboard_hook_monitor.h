// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_WIN_KEYBOARD_HOOK_MONITOR_H_
#define UI_EVENTS_WIN_KEYBOARD_HOOK_MONITOR_H_

#include "base/component_export.h"

namespace ui {

class KeyboardHookObserver;

// Provides a way to receive notifications when a low-level keyboard hook is
// registered or unregistered and to query hook registration status.
// Note that the KeyboardHookMonitor impl is bound to the thread which creates
// it.  In the browser process, this will be the UI thread.  All methods should
// be called on that thread and all observer methods will be run on that thread.
class COMPONENT_EXPORT(KEYBOARD_HOOK) KeyboardHookMonitor {
 public:
  static KeyboardHookMonitor* GetInstance();

  // Add an observer which will receive keyboard hook event notifications.  All
  // |observer| methods are called on the thread which creates the
  // KeyboardHookMonitor instance.
  virtual void AddObserver(KeyboardHookObserver* observer) = 0;

  // Remove an observer so that it will no longer receive keyboard hook event
  // notifications.
  virtual void RemoveObserver(KeyboardHookObserver* observer) = 0;

  // Indicates whether a keyboard hook has been registered and is ready to
  // receive keyboard events.
  virtual bool IsActive() = 0;

 protected:
  virtual ~KeyboardHookMonitor() = default;
};

}  // namespace ui

#endif  // UI_EVENTS_WIN_KEYBOARD_HOOK_MONITOR_H_
