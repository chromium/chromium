// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_VIRTUAL_KEYBOARD_DEBOUNCE_TIMER_H_
#define UI_BASE_IME_WIN_VIRTUAL_KEYBOARD_DEBOUNCE_TIMER_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ui {

// This class debounces a method call to the on-screen keyboard show/hide.
// This is used to throttle the TryShow/TryHide system API calls to OS and it
// doesn't introduce a strong guarantee about the keyboard visibility itself as
// that is ultimately controlled by the OS input service.
class VirtualKeyboardDebounceTimer {
 public:
  explicit VirtualKeyboardDebounceTimer(int delay_ms);

  ~VirtualKeyboardDebounceTimer();

  // Request |callback| to be invoked after the debouncing delay. If called
  // while a previous request is still pending, the previous request will be
  // cancelled.
  void RequestRun(base::OnceClosure callback);

  // Cancels any pending request.
  void CancelRequest();

 private:
  // This is called when the |timer_| expires.
  // It then invokes the |TryShow()|/|TryHide()| API based on the last reported
  // state of |VirtualKeyboardVisibilityRequest|.
  void HandleLastVirtualKeyboardVisibility();

  // The debounce delay.
  int delay_ms_ = 0;

  // Tracks when to next invoke |callback_|.
  base::OneShotTimer timer_;

  // The last time |Run| was invoked.
  base::TimeTicks time_last_run_;

  // The callback to invoke once |timer_| expires.
  base::OnceClosure callback_;
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_VIRTUAL_KEYBOARD_DEBOUNCE_TIMER_H_
