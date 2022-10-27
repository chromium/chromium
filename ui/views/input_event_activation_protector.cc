// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_event_activation_protector.h"

#include "ui/events/event.h"
#include "ui/views/metrics.h"

namespace views {

namespace {
bool g_disable_for_testing = false;
}  // namespace

void InputEventActivationProtector::VisibilityChanged(bool is_visible) {
  if (is_visible)
    view_shown_time_stamp_ = absl::make_optional<base::TimeTicks>();
}

void InputEventActivationProtector::UpdateViewShownTimeStamp() {
  if (view_shown_time_stamp_.has_value())
    view_shown_time_stamp_ = base::TimeTicks::Now();
}

bool InputEventActivationProtector::IsPossiblyUnintendedInteraction(
    const ui::Event& event) {
  if (g_disable_for_testing)
    return false;

  if (!view_shown_time_stamp_.has_value()) {
    // The UI was never shown, ignore. This can happen in tests.
    return false;
  }

  // Input event in between of visibility state changed and actual frame
  // presented.
  if (view_shown_time_stamp_.value() == base::TimeTicks()) {
    return true;
  }

  // Don't let key repeats close the dialog, they might've been held when the
  // dialog pops up.
  if (event.IsKeyEvent() && event.AsKeyEvent()->is_repeat())
    return true;

  if (!event.IsMouseEvent() && !event.IsTouchEvent())
    return false;

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

  // Unintended if the user clicked right after the UI showed.
  return event.time_stamp() < view_shown_time_stamp_.value() + kShortInterval;
}

void InputEventActivationProtector::ResetForTesting() {
  view_shown_time_stamp_.reset();
  last_event_timestamp_ = base::TimeTicks();
  repeated_event_count_ = 0;
}

// static
void InputEventActivationProtector::DisableForTesting() {
  g_disable_for_testing = true;
}

// static
bool InputEventActivationProtector::IsDisabledForTesting() {
  return g_disable_for_testing;
}

}  // namespace views
