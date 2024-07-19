// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACTIONS_ACTION_VIEW_INTERFACE_H_
#define UI_VIEWS_ACTIONS_ACTION_VIEW_INTERFACE_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "ui/actions/actions.h"
#include "ui/views/views_export.h"

namespace actions {
class ActionItem;
}

namespace views {

// See README.md for how to create an ActionViewInterface.
class VIEWS_EXPORT ActionViewInterface {
 public:
  ActionViewInterface() = default;
  virtual ~ActionViewInterface() = default;
  // Make any changes to the view when the ActionItem changes.
  virtual void ActionItemChangedImpl(actions::ActionItem* action_item) {}
  // Used to specify how the view can trigger the action.
  virtual void LinkActionInvocationToView(
      base::RepeatingClosure trigger_action_callback) {}
  // Triggers the action associated with the ActionItem.
  virtual void InvokeActionImpl(actions::ActionItem* action_item);
  // Respond to any view changes such as updating ActionItem properties.
  virtual void OnViewChangedImpl(actions::ActionItem* action_item) {}
};

}  // namespace views

#endif  // UI_VIEWS_ACTIONS_ACTION_VIEW_INTERFACE_H_
