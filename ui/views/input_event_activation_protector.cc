// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_event_activation_protector.h"

#include "ui/events/event.h"
#include "ui/views/metrics.h"

namespace views {

void InputEventActivationProtector::VisibilityChanged(bool is_visible) {
  if (is_visible)
    view_shown_time_stamp_ = base::TimeTicks::Now();
}

bool InputEventActivationProtector::IsPossiblyUnintendedInteraction(
    const ui::Event& event) {
  if (view_shown_time_stamp_ == base::TimeTicks()) {
    // The UI was never shown, ignore. This can happen in tests.
    return false;
  }

  if (!event.IsMouseEvent() && !event.IsTouchEvent())
    return false;

  const base::TimeDelta kShortInterval =
      base::TimeDelta::FromMilliseconds(GetDoubleClickInterval());
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
  return event.time_stamp() < view_shown_time_stamp_ + kShortInterval;
}

void InputEventActivationProtector::ResetForTesting() {
  view_shown_time_stamp_ = base::TimeTicks();
  last_event_timestamp_ = base::TimeTicks();
  repeated_event_count_ = 0;
}

}  // namespace views
