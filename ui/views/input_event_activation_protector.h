// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_EVENT_ACTIVATION_PROTECTOR_H_
#define UI_VIEWS_INPUT_EVENT_ACTIVATION_PROTECTOR_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "ui/views/views_export.h"

namespace ui {
class Event;
}

namespace views {

// The goal of this class is to prevent potentially unintentional user
// interaction with a UI element.
class VIEWS_EXPORT InputEventActivationProtector {
 public:
  InputEventActivationProtector() = default;
  ~InputEventActivationProtector() = default;

  // Updates the state of the protector based off of visibility changes. This
  // method must be called when the visibility of the view is changed.
  void VisibilityChanged(bool is_visible);

  // Returns true if the event is a mouse, touch, or pointer event that took
  // place within the double-click time interval after |view_shown_time_stamp_|.
  bool IsPossiblyUnintendedInteraction(const ui::Event& event);

  // Resets the state for click tracking.
  void ResetForTesting();

 private:
  // Timestamp of when the view being tracked is first shown.
  base::TimeTicks view_shown_time_stamp_;
  // Timestamp of the last event.
  base::TimeTicks last_event_timestamp_;
  // Number of repeated UI events with short intervals.
  size_t repeated_event_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(InputEventActivationProtector);
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_EVENT_ACTIVATION_PROTECTOR_H_
