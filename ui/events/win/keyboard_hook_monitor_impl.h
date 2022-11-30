// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_WIN_KEYBOARD_HOOK_MONITOR_IMPL_H_
#define UI_EVENTS_WIN_KEYBOARD_HOOK_MONITOR_IMPL_H_

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/observer_list_threadsafe.h"
#include "base/threading/thread_checker.h"
#include "ui/events/win/keyboard_hook_monitor.h"
#include "ui/events/win/keyboard_hook_observer.h"

namespace ui {

// An implementation of the KeyboardHookMonitor interface which is used to
// notify listeners of KeyboardHook registration events and provide status.
// Note that this class is bound to the thread which creates it.  In the browser
// process, this will be the UI thread.  All methods should be called on that
// thread and all observer methods will be run on that thread.
class COMPONENT_EXPORT(KEYBOARD_HOOK) KeyboardHookMonitorImpl
    : public KeyboardHookMonitor {
 public:
  static KeyboardHookMonitorImpl* GetInstance();

  KeyboardHookMonitorImpl(const KeyboardHookMonitorImpl&) = delete;
  KeyboardHookMonitorImpl& operator=(const KeyboardHookMonitorImpl&) = delete;
  KeyboardHookMonitorImpl(KeyboardHookMonitorImpl&&) = delete;
  KeyboardHookMonitorImpl& operator=(KeyboardHookMonitorImpl&&) = delete;
  ~KeyboardHookMonitorImpl() override;

  // KeyboardHookMonitor implementation.
  void AddObserver(KeyboardHookObserver* observer) override;
  void RemoveObserver(KeyboardHookObserver* observer) override;
  bool IsActive() override;

  // Send a notification to all listeners in |observers_| to indicate that a
  // keyboard hook has been registered and is listening for key events.
  void NotifyHookRegistered();

  // Send a notification to all listeners in |observers_| to indicate that a
  // keyboard hook has been removed and is no longer listening to key events.
  void NotifyHookUnregistered();

 private:
  friend base::NoDestructor<KeyboardHookMonitorImpl>;
  KeyboardHookMonitorImpl();

  bool is_hook_active_ = false;
  base::ObserverList<KeyboardHookObserver> observers_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ui

#endif  // UI_EVENTS_WIN_KEYBOARD_HOOK_MONITOR_IMPL_H_
