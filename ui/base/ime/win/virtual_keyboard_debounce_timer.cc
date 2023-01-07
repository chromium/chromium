// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/virtual_keyboard_debounce_timer.h"

namespace ui {

VirtualKeyboardDebounceTimer::VirtualKeyboardDebounceTimer(int delay_ms)
    : delay_ms_(delay_ms) {}

VirtualKeyboardDebounceTimer::~VirtualKeyboardDebounceTimer() = default;

void VirtualKeyboardDebounceTimer::RequestRun(base::OnceClosure callback) {
  // Null callback isn't a valid scenario.
  DCHECK(callback);
  callback_ = std::move(callback);
  base::TimeDelta delay(base::Milliseconds(delay_ms_));
  delay -= base::TimeTicks::Now() - time_last_run_;
  // If delay is <= 0, then it is run immediately asynchronously.
  timer_.Start(
      FROM_HERE, delay, this,
      &VirtualKeyboardDebounceTimer::HandleLastVirtualKeyboardVisibility);
}

void VirtualKeyboardDebounceTimer::CancelRequest() {
  timer_.Stop();
}

void VirtualKeyboardDebounceTimer::HandleLastVirtualKeyboardVisibility() {
  // Based on the state call the respective show/hide
  time_last_run_ = base::TimeTicks::Now();
  if (callback_)
    std::move(callback_).Run();
}

}  // namespace ui
