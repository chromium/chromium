// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_DEFAULT_INPUT_PROTECTION_POLICY_H_
#define UI_VIEWS_INPUT_PROTECTION_DEFAULT_INPUT_PROTECTION_POLICY_H_

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/views/input_protection/input_protection_policy.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {

class View;
class InputEventActivationProtector;

// Default implementation of `InputProtectionPolicy` that implements standard
// timing-based protection. This policy is used by default when no custom
// policy is passed to `InputEventActivationProtector`.
//
// It blocks input events in the following cases:
//
// 1. Key repeat events (to prevent held keys from triggering actions).
// 2. Events that occur too quickly after a previous event (to prevent
// click-spam).
// 3. Events that occur too quickly after the protection starts, which usually
// happens when the view becomes visible or resets (to prevent unintentional
// clicks immediately after a UI change).
class VIEWS_EXPORT DefaultInputProtectionPolicy : public InputProtectionPolicy,
                                                  public ViewObserver {
 public:
  // If `protected_view` is provided, the policy will automatically observe
  // its visibility changes to start and stop protection.
  explicit DefaultInputProtectionPolicy(View* protected_view = nullptr);
  ~DefaultInputProtectionPolicy() override;

  // InputProtectionPolicy:
  bool IsPossiblyUnintendedInteraction(
      const ui::Event& event,
      const View* target_view,
      const InputEventActivationProtector& protector) override;
  void OnProtectionStarted() override;
  void OnProtectionStopped() override;
  void OnProtectionReset() override;

 private:
  // Timestamp of when the view was initially protected. Used to prevent
  // unintentional user interaction event immediately from the timestamp.
  base::TimeTicks view_protected_time_stamp_;
  // Timestamp of the last event.
  base::TimeTicks last_event_timestamp_;
  // Number of repeated UI events with short intervals.
  size_t repeated_event_count_ = 0;

  // ViewObserver:
  void OnViewVisibilityChanged(View* observed_view,
                               View* starting_view,
                               bool visible) override;
  void OnViewIsDeleting(View* observed_view) override;

 private:
  base::ScopedObservation<View, ViewObserver> view_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_DEFAULT_INPUT_PROTECTION_POLICY_H_
