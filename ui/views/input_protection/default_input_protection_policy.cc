// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/default_input_protection_policy.h"

#include "ui/events/event.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/metrics.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

DefaultInputProtectionPolicy::DefaultInputProtectionPolicy(
    View* protected_view) {
  if (protected_view) {
    view_observation_.Observe(protected_view);
    if (protected_view->IsDrawn() && protected_view->GetWidget() &&
        protected_view->GetWidget()->IsVisible()) {
      OnProtectionStarted();
    }
  }
}

DefaultInputProtectionPolicy::~DefaultInputProtectionPolicy() = default;

bool DefaultInputProtectionPolicy::IsPossiblyUnintendedInteraction(
    const ui::Event& event,
    const View* target_view,
    const InputEventActivationProtector& protector) {
  if (view_protected_time_stamp_ == base::TimeTicks()) {
    // The UI was never shown, ignore. This can happen in tests.
    return false;
  }

  // Block key repeat events, as they may be a continuation of key presses that
  // started before the protection period began.
  if (event.IsKeyEvent() && event.AsKeyEvent()->is_repeat()) {
    return true;
  }

  // Update rapid event state.
  const base::TimeDelta kShortInterval = protector.cooldown_interval();
  const bool short_event_after_last_event =
      event.time_stamp() < last_event_timestamp_ + kShortInterval;
  last_event_timestamp_ = event.time_stamp();

  if (short_event_after_last_event) {
    repeated_event_count_++;
  } else {
    repeated_event_count_ = 0;
  }

  // Unintended if the user has been interacting with short intervals.
  if (repeated_event_count_ > 0) {
    return true;
  }

  // Unintended if the interaction occurred right after the view was protected.
  return event.time_stamp() <
         view_protected_time_stamp_ + protector.cooldown_interval();
}

void DefaultInputProtectionPolicy::OnProtectionStarted() {
  view_protected_time_stamp_ = base::TimeTicks::Now();
}

void DefaultInputProtectionPolicy::OnProtectionStopped() {
  view_protected_time_stamp_ = base::TimeTicks();
  last_event_timestamp_ = base::TimeTicks();
  repeated_event_count_ = 0;
}

void DefaultInputProtectionPolicy::OnProtectionReset() {
  if (!view_protected_time_stamp_.is_null()) {
    view_protected_time_stamp_ = base::TimeTicks::Now();
  }
}

void DefaultInputProtectionPolicy::OnViewVisibilityChanged(View* observed_view,
                                                           View* starting_view,
                                                           bool visible) {
  if (visible) {
    OnProtectionStarted();
  } else {
    OnProtectionStopped();
  }
}

void DefaultInputProtectionPolicy::OnViewIsDeleting(View* observed_view) {
  view_observation_.Reset();
}

}  // namespace views
