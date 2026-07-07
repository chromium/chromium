// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_event_activation_protector.h"

#include <utility>

#include "base/command_line.h"
#include "ui/events/event.h"
#include "ui/views/input_protection/default_input_protector_delegate.h"
#include "ui/views/input_protection/input_protector_delegate.h"
#include "ui/views/metrics.h"
#include "ui/views/views_switches.h"

namespace views {

InputEventActivationProtector::InputEventActivationProtector() {
  WindowsStationarityMonitor::GetInstance()->AddObserver(this);
  AddDelegate(std::make_unique<DefaultInputProtectorDelegate>());
}

InputEventActivationProtector::InputEventActivationProtector(
    std::unique_ptr<InputProtectorDelegate> delegate) {
  WindowsStationarityMonitor::GetInstance()->AddObserver(this);
  AddDelegate(std::move(delegate));
}

InputEventActivationProtector::~InputEventActivationProtector() {
  WindowsStationarityMonitor::GetInstance()->RemoveObserver(this);
}

void InputEventActivationProtector::VisibilityChanged(bool is_visible) {
  if (is_visible) {
    view_protected_time_stamp_ = base::TimeTicks::Now();
  }
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
    const ui::Event& event,
    bool allow_key_events,
    const View* target_view) {
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
    if (allow_key_events || !event.IsKeyEvent()) {
      return false;
    }
  }

  // Update the protector state for the given event and ask delegates if the
  // interaction should be blocked.
  UpdateStateForEvent(event);

  for (const auto& delegate : delegates_) {
    if (delegate->IsPossiblyUnintendedInteraction(event, target_view, this)) {
      return true;
    }
  }

  return false;
}

void InputEventActivationProtector::AddDelegate(
    std::unique_ptr<InputProtectorDelegate> delegate) {
  delegates_.push_back(std::move(delegate));
}

void InputEventActivationProtector::OnWindowStationaryStateChanged() {
  MaybeUpdateViewProtectedTimeStamp();
}

void InputEventActivationProtector::ResetForTesting() {
  view_protected_time_stamp_ = base::TimeTicks();
  last_event_timestamp_ = base::TimeTicks();
  repeated_event_count_ = 0;
}

void InputEventActivationProtector::UpdateStateForEvent(
    const ui::Event& event) {
  const base::TimeDelta kShortInterval = GetDoubleClickInterval();
  const bool short_event_after_last_event =
      event.time_stamp() < last_event_timestamp_ + kShortInterval;
  last_event_timestamp_ = event.time_stamp();

  if (short_event_after_last_event) {
    repeated_event_count_++;
  } else {
    repeated_event_count_ = 0;
  }
}

}  // namespace views
