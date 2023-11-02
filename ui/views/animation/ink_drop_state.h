// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_STATE_H_
#define UI_VIEWS_ANIMATION_INK_DROP_STATE_H_

#include <iosfwd>
#include <string>

#include "ui/views/views_export.h"

namespace views {

// The different states that the ink drop animation can be animated to.
enum class InkDropState {
  // The ink drop is not visible.
  HIDDEN,
  // The view is being interacted with but the action to be triggered has not
  // yet been determined, e.g. a mouse button down.
  ACTION_PENDING,
  // The quick action for the view has been triggered, e.g. a tap gesture or a
  // mouse click on a button.
  ACTION_TRIGGERED,
  // A view is being interacted with and the pending action will be a secondary
  // action, e.g. a long press.
  ALTERNATE_ACTION_PENDING,
  // The alternate action for the view has been triggered, e.g. a long press
  // release to bring up a menu.
  ALTERNATE_ACTION_TRIGGERED,
  // An active state for a view that is not currently being interacted with.
  // e.g. a pressed button that is showing a menu.
  ACTIVATED,
  // A previously active state has been toggled to inactive, e.g. a drop down
  // menu is closed.
  DEACTIVATED,
};

// Returns a human readable string for |state|.  Useful for logging.
VIEWS_EXPORT std::string ToString(InkDropState state);

// This is declared here for use in gtest-based unit tests but is defined in
// the views_test_support target. Depend on that to use this in your unit test.
// This should not be used in production code - call ToString() instead.
void PrintTo(InkDropState ink_drop_state, ::std::ostream* os);

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_STATE_H_
