// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_WIN_KEYBOARD_HOOK_OBSERVER_H_
#define UI_EVENTS_WIN_KEYBOARD_HOOK_OBSERVER_H_

#include "base/component_export.h"

namespace ui {

// Used in conjunction with the KeyboardHookMonitor class to receive
// notifications when a low-level keyboard hook is registered or unregistered.
class COMPONENT_EXPORT(KEYBOARD_HOOK) KeyboardHookObserver
    : public base::CheckedObserver {
 public:
  // Called when a low-level keyboard hook is registered.
  virtual void OnHookRegistered() {}

  // Called when a low-level keyboard hook is unregistered.
  virtual void OnHookUnregistered() {}

 protected:
  ~KeyboardHookObserver() override = default;
};

}  // namespace ui

#endif  // UI_EVENTS_WIN_KEYBOARD_HOOK_OBSERVER_H_
