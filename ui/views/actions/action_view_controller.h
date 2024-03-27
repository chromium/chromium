// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACTIONS_ACTION_VIEW_CONTROLLER_H_
#define UI_VIEWS_ACTIONS_ACTION_VIEW_CONTROLLER_H_

#include <map>
#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "ui/actions/actions.h"
#include "ui/views/view_tracker.h"
#include "ui/views/views_export.h"

// ActionViewController is the main view controller to be instantiated or
// subclassed. See README.md for more details.
namespace views {

class View;

// ActionViewControllerBase provides a base class for the templated
// ActionViewControllerTemplate.
class VIEWS_EXPORT ActionViewControllerBase {
 public:
  ActionViewControllerBase() = default;
  ActionViewControllerBase(const ActionViewControllerBase&) = delete;
  ActionViewControllerBase& operator=(const ActionViewControllerBase&) = delete;
  virtual ~ActionViewControllerBase() = default;
};

// ActionViewControllerTemplate is the templated core functionality that manages
// the relationship between the action item and the view. The template allows
// the action view controller to be generalized to any view class.
template <typename ViewT>
class VIEWS_EXPORT ActionViewControllerTemplate
    : public ActionViewControllerBase {
 public:
  ActionViewControllerTemplate() = default;
  ActionViewControllerTemplate(ViewT* view,
                               base::WeakPtr<actions::ActionItem> action_item) {
    SetActionView(view);
    SetActionItem(action_item);
  }
  explicit ActionViewControllerTemplate(ViewT* view) { SetActionView(view); }
  ActionViewControllerTemplate(const ActionViewControllerTemplate&) = delete;
  ActionViewControllerTemplate& operator=(const ActionViewControllerTemplate&) =
      delete;
  ~ActionViewControllerTemplate() override = default;

  void ActionItemChanged() {
    ViewT* action_view = GetActionView();
    actions::ActionItem* action_item = GetActionItem();
    if (!action_view || !action_item) {
      return;
    }
    action_view->GetActionViewInterface()->ActionItemChangedImpl(action_item);
  }

  void SetActionItem(base::WeakPtr<actions::ActionItem> action_item) {
    if (GetActionItem() == action_item.get()) {
      return;
    }
    action_item_ = action_item;
    action_changed_subscription_ = {};
    LinkActionItemAndView();
  }

  void LinkActionItemAndView() {
    ViewT* action_view = GetActionView();
    actions::ActionItem* action_item = GetActionItem();
    if (!action_item || !action_view) {
      return;
    }
    action_changed_subscription_ =
        action_item->AddActionChangedCallback(base::BindRepeating(
            &ActionViewControllerTemplate<ViewT>::ActionItemChanged,
            base::Unretained(this)));
    ActionItemChanged();
  }

  void InvokeAction() {
    actions::ActionItem* action_item = GetActionItem();
    ViewT* action_view = GetActionView();
    if (!action_item || !action_view) {
      return;
    }
    action_view->GetActionViewInterface()->InvokeActionImpl(action_item);
  }

  ViewT* GetActionView() {
    return static_cast<ViewT*>(action_view_tracker_.view());
  }

  void SetActionView(ViewT* action_view) {
    if (GetActionView() == action_view) {
      return;
    }
    action_view_tracker_.SetView(action_view);
    // base::Unretained is okay because by view controller patterns, view
    // controllers must outlive the views they manage.
    action_view->GetActionViewInterface()->LinkActionInvocationToView(
        base::BindRepeating(&ActionViewControllerTemplate::InvokeAction,
                            base::Unretained(this)));
    view_changed_subscription_ =
        action_view->RegisterNotifyViewControllerCallback(
            base::BindRepeating(&ActionViewControllerTemplate::OnViewChanged,
                                base::Unretained(this)));
    LinkActionItemAndView();
  }

  void OnViewChanged() {
    actions::ActionItem* action_item = GetActionItem();
    ViewT* action_view = GetActionView();
    if (!action_item || !action_view) {
      return;
    }
    action_view->GetActionViewInterface()->OnViewChangedImpl(action_item);
  }

  actions::ActionItem* GetActionItem() { return action_item_.get(); }

 private:
  views::ViewTracker action_view_tracker_;
  base::WeakPtr<actions::ActionItem> action_item_ = nullptr;
  base::CallbackListSubscription action_changed_subscription_;
  base::CallbackListSubscription view_changed_subscription_;
};

class VIEWS_EXPORT ActionViewController {
 public:
  ActionViewController();
  ActionViewController(const ActionViewController&) = delete;
  ActionViewController& operator=(const ActionViewController&) = delete;
  virtual ~ActionViewController();

  template <typename ViewT>
  void CreateActionViewRelationship(
      ViewT* view,
      base::WeakPtr<actions::ActionItem> action_item) {
    std::unique_ptr<ActionViewControllerTemplate<ViewT>> controller =
        std::make_unique<ActionViewControllerTemplate<ViewT>>(view,
                                                              action_item);
    action_view_controller_templates_[view] = std::move(controller);
  }

  std::map<View*, std::unique_ptr<ActionViewControllerBase>>
      action_view_controller_templates_;
};

}  // namespace views

#endif  // UI_VIEWS_ACTIONS_ACTION_VIEW_CONTROLLER_H_
