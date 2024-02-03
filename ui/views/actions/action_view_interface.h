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

// ///////////////////////////////////////////////////////////////////////////
// How to create an ActionViewInterface:
// ViewType: Type of your View Class

// Step 1: Override View::GetActionViewInterface

// std::unique_ptr<ActionViewInterface> ViewType::GetActionViewInterface()
// override {
//   return std::make_unique<ViewTypeActionViewInterface>(this);
// }

// Step 2: Create the ActionViewInterface

// Instead of BaseActionViewInterface, subclass the ActionViewInterface subclass
// associated with the parent view class to get action behaviors of the parent
// class.

// class ViewTypeActionViewInterface : public BaseActionViewInterface {
//  public:
//   explicit ViewTypeActionViewInterface(ViewType* action_view)
//      : BaseActionViewInterface(action_view), action_view_(action_view) {}
//   ~ViewTypeActionViewInterface() override = default;
//
//   // optional: override virtual methods.

//  private:
//   raw_ptr<ViewType> action_view_;
// };
class VIEWS_EXPORT ActionViewInterface {
 public:
  ActionViewInterface() = default;
  virtual ~ActionViewInterface() = default;
  virtual void ActionItemChangedImpl(actions::ActionItem* action_item) {}
  virtual void LinkActionInvocationToView(
      base::RepeatingClosure trigger_action_callback) {}
  virtual void InvokeActionImpl(actions::ActionItem* action_item);
  virtual void OnViewChangedImpl(actions::ActionItem* action_item) {}
};

}  // namespace views

#endif  // UI_VIEWS_ACTION_VIEW_INTERFACE_H_
