// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_EVENT_ACTIVATION_PROTECTOR_H_
#define UI_VIEWS_INPUT_EVENT_ACTIVATION_PROTECTOR_H_

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

  InputEventActivationProtector(const InputEventActivationProtector&) = delete;
  InputEventActivationProtector& operator=(
      const InputEventActivationProtector&) = delete;

  ~InputEventActivationProtector() = default;

  // Initialize the |view_shown_time_stamp_| of the protector based off of
  // visibility changes. This method must be called when the visibility of the
  // view is changed.
  void VisibilityChanged(bool is_visible);

  // Updates the |view_shown_time_stamp_| based on "draw" state of the owner
  // view. This method basically will be called when a frame of the view is
  // presented.
  void UpdateViewShownTimeStamp();

  // Returns true if the event is a mouse, touch, or pointer event that took
  // place within the double-click time interval after |view_shown_time_stamp_|.
  bool IsPossiblyUnintendedInteraction(const ui::Event& event);

  // Resets the state for click tracking.
  void ResetForTesting();

  // Integration tests can disable all input event activation protectors.
  static void DisableForTesting();
  static bool IsDisabledForTesting();

 private:
  // Timestamp of when the view being tracked is first shown.
  absl::optional<base::TimeTicks> view_shown_time_stamp_;
  // Timestamp of the last event.
  base::TimeTicks last_event_timestamp_;
  // Number of repeated UI events with short intervals.
  size_t repeated_event_count_ = 0;
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_EVENT_ACTIVATION_PROTECTOR_H_
