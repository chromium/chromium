// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/occlusion_aware_input_protection_delegate.h"

#include "base/check.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/input_protection/occluded_widget_input_protector.h"
#include "ui/views/widget/widget.h"

namespace views {

OcclusionAwareInputProtectionDelegate::OcclusionAwareInputProtectionDelegate(
    Widget* widget)
    : widget_(widget) {
  CHECK(widget_);
  observation_.Observe(widget_);
}

OcclusionAwareInputProtectionDelegate::
    ~OcclusionAwareInputProtectionDelegate() = default;

void OcclusionAwareInputProtectionDelegate::OnWidgetVisibilityChanged(
    Widget* widget,
    bool visible) {
  // TODO(crbug.com/467460499): Implement visibility tracking for show cooldown.
}

void OcclusionAwareInputProtectionDelegate::OnWidgetActivationChanged(
    Widget* widget,
    bool active) {
  // TODO(crbug.com/467460499): Implement activation tracking for activation
  // cooldown.
}

void OcclusionAwareInputProtectionDelegate::OnWidgetDestroying(Widget* widget) {
  observation_.Reset();
  widget_ = nullptr;
}

bool OcclusionAwareInputProtectionDelegate::IsPossiblyUnintendedInteraction(
    const ui::Event& event,
    const View* target_view,
    InputEventActivationProtector* protector) {
  // TODO(crbug.com/467460499): Once `target_view` is passed by reference
  // (`const View&`) in the interface, this `CHECK` can be removed.
  CHECK(target_view);

  if (!widget_ || !widget_->IsVisible()) {
    return false;
  }

  // TODO(crbug.com/467460499): Implement timing check (cooldown after
  // show/activate).

  if (OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
          event, *target_view)) {
    return true;
  }

  return false;
}

}  // namespace views
