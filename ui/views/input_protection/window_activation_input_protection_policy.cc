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
  Widget* parent = widget->GetPrimaryWindowWidget();
  if (!parent || parent == widget) {
    return;
  }

  widget_observation_.Observe(widget);
  parent_observation_.Observe(parent);
  needs_activation_protection_ = !parent->IsVisible();
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
  if (widget != widget_observation_.GetSource()) {
    return;
  }
  if (!active || !needs_activation_protection_) {
    return;
  }

  widget_protected_time_stamp_ = base::TimeTicks::Now();
  needs_activation_protection_ = false;
}

void WindowActivationInputProtectionPolicy::OnWidgetVisibilityChanged(
    Widget* widget,
    bool visible) {
  if (widget != parent_observation_.GetSource() || visible) {
    return;
  }

  needs_activation_protection_ = true;
}

void WindowActivationInputProtectionPolicy::OnWidgetDestroying(Widget* widget) {
  if (widget_observation_.IsObservingSource(widget)) {
    widget_observation_.Reset();
  }
  if (parent_observation_.IsObservingSource(widget)) {
    parent_observation_.Reset();
  }
}

}  // namespace views
