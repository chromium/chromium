// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_INTERFACE_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_INTERFACE_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace views {
class MenuButtonController;

namespace internal {

// An abstract interface for menu runner implementations.
// Invoke Release() to destroy. Release() deletes immediately if the menu isn't
// showing. If the menu is showing Release() cancels the menu and when the
// nested RunMenuAt() call returns deletes itself and the menu.
class MenuRunnerImplInterface {
 public:
  // Creates a concrete instance for running |menu_model|.
  // |run_types| is a bitmask of MenuRunner::RunTypes.
  static MenuRunnerImplInterface* Create(
      ui::MenuModel* menu_model,
      int32_t run_types,
      base::RepeatingClosure on_menu_closed_callback);

  // Returns true if we're in a nested run loop running the menu.
  virtual bool IsRunning() const = 0;

  // See description above class for details.
  virtual void Release() = 0;

  // Runs the menu. See MenuRunner::RunMenuAt for more details.
  virtual void RunMenuAt(Widget* parent,
                         MenuButtonController* button_controller,
                         const gfx::Rect& bounds,
                         MenuAnchorPosition anchor,
                         int32_t run_types) = 0;

  // Hides and cancels the menu.
  virtual void Cancel() = 0;

  // Returns the time from the event which closed the menu - or 0.
  virtual base::TimeTicks GetClosingEventTime() const = 0;

 protected:
  // Call Release() to delete.
  virtual ~MenuRunnerImplInterface() = default;
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_INTERFACE_H_
