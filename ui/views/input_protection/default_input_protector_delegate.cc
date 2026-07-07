// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/default_input_protector_delegate.h"

#include "ui/events/event.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/metrics.h"

namespace views {

bool DefaultInputProtectorDelegate::IsPossiblyUnintendedInteraction(
    const ui::Event& event,
    const View* target_view,
    InputEventActivationProtector* protector) {
  // Unintended if the user has been clicking with short intervals.
  if (protector->repeated_event_count() > 0) {
    return true;
  }

  // Unintended if the user clicked right after the view was protected.
  return event.time_stamp() <
         protector->view_protected_time_stamp() + GetDoubleClickInterval();
}

}  // namespace views
