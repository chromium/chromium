// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACTION_VIEW_CONTROLLER_H_
#define UI_VIEWS_ACTION_VIEW_CONTROLLER_H_

#include <map>
#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "ui/actions/actions.h"
#include "ui/views/view_tracker.h"
#include "ui/views/views_export.h"

/////////////////////////////////////////////////////////////////////////////
//
// How to allow ActionViewController to support a new view class:
// (See ui/views/controls/button/button files for a concrete example.)
//
// ClassName: Name of your class
// ParentName: Name of the parent class with already specialized functionality.
// This may just be `View`. Parent class specialization will also be propagated
// to child views.
//
// Add the following in the .h file of view class:
//
// #include "ui/views/action_view_controller.h"
//
// template <>
// struct VIEWS_EXPORT ActionViewControllerSuperClassT<ClassName> {
//   using SuperClass = ActionViewControllerTemplate<ParentName>;
// };
//
// The following are optional, but can be specialized as desired. Declare in the
// .h file of the view class with functionality defined in .cc file.

// If you wish to add class specific functionality for responding to action
// items changing:
//
// template <>
// void ActionViewControllerTemplate<ClassName,
//                                   ActionViewControllerTemplate<ParentName>>::
//     ActionItemChangedImpl(ClassName* action_view,
//                           actions::ActionItem* action_item);
//
// If you wish to add class specific functionality for when the action view is
// set:
//
// template <>
// void ActionViewControllerTemplate<ClassName,
//                                   ActionViewControllerTemplate<ParentName>>::
//     SetActionViewImpl(ClassName* action_view);

namespace views {

class View;

// Base action view controller that provides the interface for a standard action
// view controller.
class VIEWS_EXPORT ActionViewControllerBase {
 public:
  ActionViewControllerBase() = default;
  ActionViewControllerBase(const ActionViewControllerBase&) = delete;
  ActionViewControllerBase& operator=(const ActionViewControllerBase&) = delete;
  virtual ~ActionViewControllerBase() = default;
  void ActionItemChanged() {}
  void ActionItemChangedInterim(View* view, actions::ActionItem* action_item) {}
  void SetActionView(View* action_view) {}
  void SetActionItem(base::WeakPtr<actions::ActionItem> action_item) {}
};

template <typename ViewT>
struct VIEWS_EXPORT ActionViewControllerSuperClassT {
  using SuperClass = ActionViewControllerBase;
};

// ActionViewControllerTemplate is the templated core functionality that manages
// the relationship between the action item and the view. The template allows
// the action view controller to be generalized to any view class.
template <typename ViewT,
          typename SuperClassViewControllerT =
              typename ActionViewControllerSuperClassT<ViewT>::SuperClass>
class VIEWS_EXPORT ActionViewControllerTemplate
    : public SuperClassViewControllerT {
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
            &ActionViewControllerTemplate<
                ViewT, SuperClassViewControllerT>::ActionItemChanged,
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

// ActionViewController is the main view controller to be instantiated or
// subclassed. Under the hood it creates the appropriate templated
// ActionViewControllerTemplate for all classes of views.
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

#endif  // UI_VIEWS_ACTION_VIEW_CONTROLLER_H_
