// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_event_activation_protector.h"

#include "base/command_line.h"
#include "ui/events/event.h"
#include "ui/views/metrics.h"
#include "ui/views/views_switches.h"

namespace views {

InputEventActivationProtector::InputEventActivationProtector() {
  WindowsStationarityMonitor::GetInstance()->AddObserver(this);
}

InputEventActivationProtector::~InputEventActivationProtector() {
  WindowsStationarityMonitor::GetInstance()->RemoveObserver(this);
}

void InputEventActivationProtector::VisibilityChanged(bool is_visible) {
  if (is_visible)
    view_protected_time_stamp_ = base::TimeTicks::Now();
}

void InputEventActivationProtector::MaybeUpdateViewProtectedTimeStamp(
    bool force) {
  // The UI was never shown, ignore.
  if (!force && view_protected_time_stamp_ == base::TimeTicks()) {
    return;
  }

  view_protected_time_stamp_ = base::TimeTicks::Now();
}

bool InputEventActivationProtector::IsPossiblyUnintendedInteraction(
    const ui::Event& event) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableInputEventActivationProtectionForTesting))
      [[unlikely]] {
    return false;
  }

  if (view_protected_time_stamp_ == base::TimeTicks()) {
    // The UI was never shown, ignore. This can happen in tests.
    return false;
  }

  // Don't let key repeats close the dialog, they might've been held when the
  // dialog pops up.
  if (event.IsKeyEvent() && event.AsKeyEvent()->is_repeat()) {
    return true;
  }

  if (!event.IsMouseEvent() && !event.IsTouchEvent() &&
      !event.IsGestureEvent()) {
    return false;
  }

  const base::TimeDelta kShortInterval =
      base::Milliseconds(GetDoubleClickInterval());
  const bool short_event_after_last_event =
      event.time_stamp() < last_event_timestamp_ + kShortInterval;
  last_event_timestamp_ = event.time_stamp();

  // Unintended if the user has been clicking with short intervals.
  if (short_event_after_last_event) {
    repeated_event_count_++;
    return true;
  }
  repeated_event_count_ = 0;

  // Unintended if the user clicked right after the view was protected.
  return event.time_stamp() < view_protected_time_stamp_ + kShortInterval;
}

void InputEventActivationProtector::OnWindowStationaryStateChanged() {
  MaybeUpdateViewProtectedTimeStamp();
}

void InputEventActivationProtector::ResetForTesting() {
  view_protected_time_stamp_ = base::TimeTicks();
  last_event_timestamp_ = base::TimeTicks();
  repeated_event_count_ = 0;
}

}  // namespace views
