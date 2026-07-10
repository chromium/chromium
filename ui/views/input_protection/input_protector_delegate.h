// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTOR_DELEGATE_H_
#define UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTOR_DELEGATE_H_

#include "ui/views/views_export.h"

namespace ui {
class Event;
}

namespace views {

class InputEventActivationProtector;
class View;

// Delegate interface for evaluating if an input event should be blocked
// as a possibly unintended interaction. This allows incorporating additional
// signals into the protection check.
class VIEWS_EXPORT InputProtectorDelegate {
 public:
  virtual ~InputProtectorDelegate() = default;

  // Returns true if the `event` should be blocked based on the delegate's
  // logic.
  //
  // If `target_view` is provided, the delegate can use it to perform security
  // checks on the view that is the target for the event. The `protector`
  // provides access to the calling protector's state.
  //
  // TODO(crbug.com/467460499): Once all legacy callers are migrated to pass
  // `target_view`, change it to `const View&` to enforce non-nullness.
  virtual bool IsPossiblyUnintendedInteraction(
      const ui::Event& event,
      const View* target_view,
      InputEventActivationProtector* protector) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTOR_DELEGATE_H_
