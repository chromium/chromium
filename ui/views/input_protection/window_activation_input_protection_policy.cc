// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/window_activation_input_protection_policy.h"

#include "base/check.h"
#include "ui/events/event.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/widget/widget.h"

namespace views {

WindowActivationInputProtectionPolicy::WindowActivationInputProtectionPolicy(
    Widget* widget) {
  CHECK(widget);
  widget_observation_.Observe(widget);
  parent_was_visible_when_activation_changed_ = IsParentVisible(widget);
}

WindowActivationInputProtectionPolicy::
    ~WindowActivationInputProtectionPolicy() = default;

bool WindowActivationInputProtectionPolicy::IsPossiblyUnintendedInteraction(
    const ui::Event& event,
    const View* target_view,
    const InputEventActivationProtector& protector) {
  if (widget_protected_time_stamp_.is_null()) {
    return false;
  }

  return event.time_stamp() <
         widget_protected_time_stamp_ + protector.cooldown_interval();
}

void WindowActivationInputProtectionPolicy::OnProtectionReset() {
  if (!widget_protected_time_stamp_.is_null()) {
    widget_protected_time_stamp_ = base::TimeTicks::Now();
  }
}

void WindowActivationInputProtectionPolicy::OnWidgetActivationChanged(
    Widget* widget,
    bool active) {
  if (active && !parent_was_visible_when_activation_changed_) {
    widget_protected_time_stamp_ = base::TimeTicks::Now();
  }
  parent_was_visible_when_activation_changed_ = IsParentVisible(widget);
}

void WindowActivationInputProtectionPolicy::OnWidgetDestroying(Widget* widget) {
  widget_observation_.Reset();
}

bool WindowActivationInputProtectionPolicy::IsParentVisible(
    Widget* widget) const {
  if (Widget* parent = widget->GetPrimaryWindowWidget()) {
    return parent->IsVisible();
  }
  return false;
}

}  // namespace views
