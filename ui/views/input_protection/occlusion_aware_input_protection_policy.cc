// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/occlusion_aware_input_protection_policy.h"

#include "base/check.h"
#include "ui/events/event.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/input_protection/occluded_widget_input_protector.h"
#include "ui/views/widget/widget.h"

namespace views {

bool OcclusionAwareInputProtectionPolicy::IsPossiblyUnintendedInteraction(
    const ui::Event& event,
    const View* target_view,
    const InputEventActivationProtector& protector) {
  // TODO(crbug.com/467460499): Once `target_view` is passed by reference
  // (`const View&`) in the interface, this `CHECK` can be removed.
  CHECK(target_view);

  const Widget* widget = target_view->GetWidget();
  if (!widget) {
    return false;
  }

  return OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event, *target_view);
}

}  // namespace views
