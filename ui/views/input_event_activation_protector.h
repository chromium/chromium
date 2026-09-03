// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_EVENT_ACTIVATION_PROTECTOR_H_
#define UI_VIEWS_INPUT_EVENT_ACTIVATION_PROTECTOR_H_

#include <memory>
#include <vector>

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/views/views_export.h"
#include "ui/views/windows_stationarity_monitor.h"

namespace ui {
class Event;
}

namespace views {

class InputProtectionPolicy;
class View;

// The goal of this class is to prevent potentially unintentional user
// interaction with a UI element.
// See switch kDisableInputEventActivationProtectionForTesting for disabling it.
class VIEWS_EXPORT InputEventActivationProtector
    : WindowsStationarityMonitor::Observer {
 public:
  // Creates a protector with the default timing-based protection policy
  // (using `DefaultInputProtectionPolicy`).
  InputEventActivationProtector();

  // Creates a protector with a custom initial protection policy. Use this
  // when you want to replace the default timing checks with your own custom
  // logic.
  explicit InputEventActivationProtector(
      std::unique_ptr<InputProtectionPolicy> policy);
  ~InputEventActivationProtector() override;

  InputEventActivationProtector(const InputEventActivationProtector&) = delete;
  InputEventActivationProtector& operator=(
      const InputEventActivationProtector&) = delete;

  // Updates the state of the protector based off of visibility changes. This
  // method must be called when the visibility of the view is changed.
  void VisibilityChanged(bool is_visible);

  // Notifies policies to reset or restart their protection window if needed.
  // This is called under certain view property changes.
  //
  // If `force` is true, forces policies to start their protection window
  // immediately (early activation). It is helpful to prevent unintentional
  // events from happening when, for example, a "visibility changed" event
  // arrives after a click event (for example click event -> tab activation ->
  // visibility change).
  void MaybeUpdateViewProtectedTimeStamp(bool force = false);

  // Returns true if the event is considered a possibly unintended interaction
  // (e.g. click-spam or inputs too close to when the view/widget was shown).
  //
  // If `allow_key_events` is true, key events will bypass the timing-based
  // protections of the default policy.
  //
  // If `target_view` is provided, policies can use it to perform security
  // checks on the view that is the target for the event.
  virtual bool IsPossiblyUnintendedInteraction(const ui::Event& event,
                                               bool allow_key_events,
                                               const View* target_view);
  bool IsPossiblyUnintendedInteraction(const ui::Event& event,
                                       bool allow_key_events) {
    return IsPossiblyUnintendedInteraction(event, allow_key_events, nullptr);
  }

  // Adds a policy to the list of policies that check for unintended
  // interactions. To allow the event for the interaction to proceed, all
  // registered policies must agree.
  void AddPolicy(std::unique_ptr<InputProtectionPolicy> policy);

  // Implements WindowsStationarityMonitor::Observer:
  void OnWindowStationaryStateChanged() override;

  // Resets the state for click tracking.
  void ResetForTesting();

  // Returns the cooldown interval used to prevent unintended interactions.
  // This serves as a baseline value so individual policies do not need to
  // define their own.
  //
  // The protection period begins when trigger conditions defined by the
  // policies are met. These include when the view becomes visible, window
  // stationarity or activation changes, occlusion by always-on-top windows
  // occurs, or a click event occurs (to prevent click-spam). During this
  // period, input events (such as mouse clicks, touches, or gestures) are
  // blocked, depending on the policy configuration.
  const base::TimeDelta& cooldown_interval() const {
    return cooldown_interval_;
  }

 private:
  // The duration of the protection period. See `cooldown_interval()` for
  // details.
  const base::TimeDelta cooldown_interval_;

  // Policies that evaluate if an interaction should be blocked.
  std::vector<std::unique_ptr<InputProtectionPolicy>> policies_;

  base::ScopedObservation<WindowsStationarityMonitor,
                          WindowsStationarityMonitor::Observer>
      stationarity_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_EVENT_ACTIVATION_PROTECTOR_H_
