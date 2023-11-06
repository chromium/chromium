// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACTION_VIEW_CONTROLLER_H_
#define UI_VIEWS_ACTION_VIEW_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "ui/actions/actions.h"
#include "ui/views/view_tracker.h"
#include "ui/views/views_export.h"

namespace views {

class View;

class VIEWS_EXPORT ActionController {
 public:
  ActionController() = default;
  ActionController(const ActionController&) = delete;
  ActionController& operator=(const ActionController&) = delete;
  virtual ~ActionController() = default;
  void ActionItemChanged() {}
  void ActionItemChangedInterim(View* view, actions::ActionItem* action_item) {}
  void SetActionView(View* action_view) {}
  void SetActionItem(base::WeakPtr<actions::ActionItem> action_item) {}
};

template <typename ViewT>
struct VIEWS_EXPORT ActionViewControllerSuperClassT {
  using SuperClass = ActionController;
};

template <typename ViewT,
          typename SuperClassViewControllerT =
              typename ActionViewControllerSuperClassT<ViewT>::SuperClass>
class VIEWS_EXPORT ActionViewController : public SuperClassViewControllerT {
 public:
  ActionViewController() = default;
  ActionViewController(ViewT* view,
                       base::WeakPtr<actions::ActionItem> action_item) {
    SetActionView(view);
    SetActionItem(action_item);
  }
  explicit ActionViewController(ViewT* view) { SetActionView(view); }
  ActionViewController(const ActionViewController&) = delete;
  ActionViewController& operator=(const ActionViewController&) = delete;
  ~ActionViewController() override = default;

  void ActionItemChanged() {
    ViewT* action_view = GetActionView();
    actions::ActionItem* action_item = GetActionItem();
    if (!action_view || !action_item) {
      return;
    }
    ActionItemChangedInterim(action_view, action_item);
  }

  void ActionItemChangedInterim(ViewT* view, actions::ActionItem* action_item) {
    SuperClassViewControllerT::ActionItemChangedInterim(view, action_item);
    ActionItemChangedImpl(view, action_item);
  }

  void SetActionItem(base::WeakPtr<actions::ActionItem> action_item) {
    if (GetActionItem() == action_item.get()) {
      return;
    }
    action_item_ = action_item;
    action_changed_subscription_ = {};
    LinkActionItemAndView();
    // Calling this method up to the super classes allows the action item to be
    // accessible at each super class level of the template.
    SuperClassViewControllerT::SetActionItem(action_item);
  }

  void LinkActionItemAndView() {
    ViewT* action_view = GetActionView();
    actions::ActionItem* action_item = GetActionItem();
    if (!action_item || !action_view) {
      return;
    }
    action_changed_subscription_ =
        action_item->AddActionChangedCallback(base::BindRepeating(
            &ActionViewController<ViewT,
                                  SuperClassViewControllerT>::ActionItemChanged,
            base::Unretained(this)));
    ActionItemChanged();
    action_view->InvalidateLayout();
    action_view->SchedulePaint();
  }

  void TriggerAction() {
    actions::ActionItem* action_item = GetActionItem();
    if (action_item) {
      action_item->InvokeAction();
    }
  }

  ViewT* GetActionView() {
    return static_cast<ViewT*>(action_view_tracker_.view());
  }

  void SetActionView(ViewT* action_view) {
    if (GetActionView() == action_view) {
      return;
    }
    action_view_tracker_.SetView(action_view);
    // Calling this method up to the super classes allows the view to be
    // accessible at each super class level of the template as well as get the
    // specific behaviors set on the view of the super classes.
    SuperClassViewControllerT::SetActionView(action_view);
    SetActionViewImpl(action_view);
    LinkActionItemAndView();
  }

  actions::ActionItem* GetActionItemForTesting() { return GetActionItem(); }

 private:
  actions::ActionItem* GetActionItem() { return action_item_.get(); }
  void SetActionViewImpl(ViewT* action_view) {}
  void ActionItemChangedImpl(ViewT* action_view,
                             actions::ActionItem* action_item);

  views::ViewTracker action_view_tracker_;
  base::WeakPtr<actions::ActionItem> action_item_ = nullptr;
  base::CallbackListSubscription action_changed_subscription_;
};

}  // namespace views

#endif  // UI_VIEWS_ACTION_VIEW_CONTROLLER_H_
