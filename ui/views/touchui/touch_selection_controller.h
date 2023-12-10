// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_H_
#define UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_H_

#include "ui/views/views_export.h"

namespace views {

// Interface for the touch selection controller, which handles touch selection
// handles and related UI for selecting/editing text using touch.
class VIEWS_EXPORT TouchSelectionController {
 public:
  virtual ~TouchSelectionController() = default;

  // Notifies the controller that the text selection has changed.
  virtual void SelectionChanged() = 0;

  // Toggles showing/hiding the touch selection menu.
  virtual void ToggleQuickMenu() = 0;
};

}  // namespace views

#endif  // UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_H_
