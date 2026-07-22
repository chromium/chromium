// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_POLICY_H_
#define UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_POLICY_H_

#include "ui/views/views_export.h"

namespace ui {
class Event;
}

namespace views {

class InputEventActivationProtector;
class View;

// Policy interface for evaluating if an input event should be blocked as a
// possibly unintended interaction. This allows incorporating additional signals
// into the protection check.
class VIEWS_EXPORT InputProtectionPolicy {
 public:
  InputProtectionPolicy() = default;
  virtual ~InputProtectionPolicy() = default;

  InputProtectionPolicy(const InputProtectionPolicy&) = delete;
  InputProtectionPolicy& operator=(const InputProtectionPolicy&) = delete;

  // Returns true if the `event` should be blocked based on the policy's logic.
  //
  // If `target_view` is provided, the policy can use it to perform security
  // checks on the view that is the target for the event. The `protector`
  // provides access to the calling protector's state.
  //
  // TODO(crbug.com/467460499): Once all legacy callers are migrated to pass
  // `target_view`, change it to `const View&` to enforce non-nullness.
  //
  // TODO(crbug.com/467460499): Consider using a more framework-agnostic
  // representation like `ui::TrackedElement` instead of `views::View` to
  // support other frameworks (e.g., WebUI, Android) in the future.
  virtual bool IsPossiblyUnintendedInteraction(
      const ui::Event& event,
      const View* target_view,
      const InputEventActivationProtector& protector) = 0;

  // Lifecycle notifications ---------------------------------------------------
  //
  // Lifecycle actions notified by the protector. Subclasses can override these
  // to respond to protector-driven lifecycle events.

  // Called when the protected target becomes visible or is shown, indicating
  // the start of the potential interaction period. Policies can use this to
  // initialize timers or record start timestamps (e.g.,
  // `DefaultInputProtectionPolicy` records the show time to start a cooldown).
  virtual void OnProtectionStarted() {}

  // Called when the protected target is hidden or destroyed, ending the
  // interaction period. Policies should use this to clear any accumulated
  // state or stop active timers.
  virtual void OnProtectionStopped() {}

  // Called when a change in the UI state (e.g., layout changes, window
  // stationarity changes) requires restarting the protection cooldown. Policies
  // should respond by restarting their protection cooldown (typically by
  // updating their stored protection timestamps to the current time).
  virtual void OnProtectionReset() {}
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_POLICY_H_
