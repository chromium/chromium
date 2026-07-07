// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_EVENT_ACTIVATION_PROTECTOR_H_
#define UI_VIEWS_INPUT_EVENT_ACTIVATION_PROTECTOR_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "ui/views/views_export.h"
#include "ui/views/windows_stationarity_monitor.h"

namespace ui {
class Event;
}

namespace views {

class InputProtectorDelegate;
class View;

// The goal of this class is to prevent potentially unintentional user
// interaction with a UI element.
// See switch kDisableInputEventActivationProtectionForTesting for disabling it.
class VIEWS_EXPORT InputEventActivationProtector
    : WindowsStationarityMonitor::Observer {
 public:
  // Creates a protector with the default timing-based protection policy
  // (using `DefaultInputProtectorDelegate`).
  InputEventActivationProtector();

  // Creates a protector with a custom initial protection policy. Use this
  // when you want to replace the default timing checks with your own custom
  // logic.
  explicit InputEventActivationProtector(
      std::unique_ptr<InputProtectorDelegate> delegate);
  ~InputEventActivationProtector() override;

  InputEventActivationProtector(const InputEventActivationProtector&) = delete;
  InputEventActivationProtector& operator=(
      const InputEventActivationProtector&) = delete;

  // Updates the state of the protector based off of visibility changes. This
  // method must be called when the visibility of the view is changed.
  void VisibilityChanged(bool is_visible);

  // Updates the |view_protected_time_stamp_| if needed. This function will be
  // called when we want to reset back the input protector to "initial
  // protected" state, basically under some certain view's proprieties changed
  // events.
  //
  // If |force| is true, force to update the |view_protected_time_stamp_| even
  // earlier (shortly before the owner view is visible). It usually helps us to
  // prevent unintentional clicks happening when "visibility changes" event
  // coming later than click event (for example click event -> tab activation ->
  // visibility change).
  void MaybeUpdateViewProtectedTimeStamp(bool force = false);

  // Returns true if the event is a mouse, touch, pointer, or key (optionally)
  // event that took place within the double-click time interval after
  // `view_protected_time_stamp_`.
  //
  // If `allow_key_events` is true, "key events" will NOT be considered for
  // possibly unintended interaction checks.
  //
  // If `target_view` is provided, delegates can use it to perform security
  // checks on the view that is the target for the event.
  virtual bool IsPossiblyUnintendedInteraction(const ui::Event& event,
                                               bool allow_key_events,
                                               const View* target_view);
  bool IsPossiblyUnintendedInteraction(const ui::Event& event,
                                       bool allow_key_events) {
    return IsPossiblyUnintendedInteraction(event, allow_key_events, nullptr);
  }

  // Adds a delegate to the list of policies that check for unintended
  // interactions. To allow the event for the interaction to proceed, all
  // registered delegates must agree.
  void AddDelegate(std::unique_ptr<InputProtectorDelegate> delegate);

  // Implements WindowsStationarityMonitor::Observer:
  void OnWindowStationaryStateChanged() override;

  // Resets the state for click tracking.
  void ResetForTesting();

  base::TimeTicks view_protected_time_stamp() const {
    return view_protected_time_stamp_;
  }

  base::TimeTicks last_event_timestamp() const { return last_event_timestamp_; }

  size_t repeated_event_count() const { return repeated_event_count_; }

 private:
  // Timestamp of when the view was initially protected. Used to prevent
  // unintentional user interaction event immediately from the timestamp.
  base::TimeTicks view_protected_time_stamp_;
  // Timestamp of the last event.
  base::TimeTicks last_event_timestamp_;
  // Number of repeated UI events with short intervals.
  size_t repeated_event_count_ = 0;

  // Updates the internal event tracking state (last event timestamp, repeated
  // event count) based on the incoming event.
  void UpdateStateForEvent(const ui::Event& event);

  // Delegates that evaluate if an interaction should be blocked.
  std::vector<std::unique_ptr<InputProtectorDelegate>> delegates_;
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_EVENT_ACTIVATION_PROTECTOR_H_
