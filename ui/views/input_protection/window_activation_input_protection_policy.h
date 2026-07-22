// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_WINDOW_ACTIVATION_INPUT_PROTECTION_POLICY_H_
#define UI_VIEWS_INPUT_PROTECTION_WINDOW_ACTIVATION_INPUT_PROTECTION_POLICY_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/views/input_protection/input_protection_policy.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

class Widget;
class View;
class InputEventActivationProtector;

// An implementation of `InputProtectionPolicy` that protects widgets against
// unintended interaction when the widget becomes active after its parent window
// was invisible. It observes the target widget to track its activation state.
//
// It blocks input events in the following case:
//
// 1. Sudden activation: The observed widget becomes active, but its parent
//    window was previously invisible. Input events are blocked during a
//    cooldown period following this activation. This protects against cases
//    where a child dialog (e.g., a permission prompt) suddenly appears and
//    takes focus, potentially intercepting a click intended for another
//    window.
class VIEWS_EXPORT WindowActivationInputProtectionPolicy final
    : public InputProtectionPolicy,
      public WidgetObserver {
 public:
  explicit WindowActivationInputProtectionPolicy(Widget* widget);
  ~WindowActivationInputProtectionPolicy() override;

  // InputProtectionPolicy:
  bool IsPossiblyUnintendedInteraction(
      const ui::Event& event,
      const View* target_view,
      const InputEventActivationProtector& protector) override;
  void OnProtectionReset() override;

  // WidgetObserver:
  void OnWidgetActivationChanged(Widget* widget, bool active) override;
  void OnWidgetDestroying(Widget* widget) override;

 private:
  // Returns true if the parent widget is currently visible.
  bool IsParentVisible(Widget* widget) const;

  // Tracks if the parent of the widget was visible when activation last
  // changed.
  bool parent_was_visible_when_activation_changed_ = true;

  // The timestamp when the widget was last protected.
  base::TimeTicks widget_protected_time_stamp_;

  // Observation of the target widget.
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_WINDOW_ACTIVATION_INPUT_PROTECTION_POLICY_H_
