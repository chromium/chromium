// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/focus_manager.h"

#include <algorithm>
#include <vector>

#include "base/auto_reset.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/focus/focus_manager_delegate.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

bool FocusManager::arrow_key_traversal_enabled_ = false;

FocusManager::FocusManager(Widget* widget,
                           std::unique_ptr<FocusManagerDelegate> delegate)
    : widget_(widget),
      delegate_(std::move(delegate)),
      view_tracker_for_stored_view_(std::make_unique<ViewTracker>()) {
  DCHECK(widget_);
}

FocusManager::~FocusManager() {
  if (focused_view_)
    focused_view_->RemoveObserver(this);
}

bool FocusManager::OnKeyEvent(const ui::KeyEvent& event) {
  const int key_code = event.key_code();

  if (event.type() != ui::ET_KEY_PRESSED && event.type() != ui::ET_KEY_RELEASED)
    return false;

  if (shortcut_handling_suspended())
    return true;

  ui::Accelerator accelerator(event);

  if (event.type() == ui::ET_KEY_PRESSED) {
    // If the focused view wants to process the key event as is, let it be.
    if (focused_view_ && focused_view_->SkipDefaultKeyEventProcessing(event) &&
        !accelerator_manager_.HasPriorityHandler(accelerator))
      return true;

    // Intercept Tab related messages for focus traversal.
    // Note that we don't do focus traversal if the root window is not part of
    // the active window hierarchy as this would mean we have no focused view
    // and would focus the first focusable view.
    if (IsTabTraversalKeyEvent(event)) {
      AdvanceFocus(event.IsShiftDown());
      return false;
    }

    if ((arrow_key_traversal_enabled_ ||
         arrow_key_traversal_enabled_for_widget_) &&
        ProcessArrowKeyTraversal(event)) {
      return false;
    }

    // Intercept arrow key messages to switch between grouped views.
    bool is_left = key_code == ui::VKEY_LEFT || key_code == ui::VKEY_UP;
    bool is_right = key_code == ui::VKEY_RIGHT || key_code == ui::VKEY_DOWN;
    if (focused_view_ && focused_view_->GetGroup() != -1 &&
        (is_left || is_right)) {
      bool next = is_right;
      View::Views views;
      focused_view_->parent()->GetViewsInGroup(focused_view_->GetGroup(),
                                               &views);
      View::Views::const_iterator i(
          std::find(views.begin(), views.end(), focused_view_));
      DCHECK(i != views.end());
      size_t index = i - views.begin();
      if (next && index == views.size() - 1)
        index = 0;
      else if (!next && index == 0)
        index = views.size() - 1;
      else
        index += next ? 1 : -1;
      SetFocusedViewWithReason(views[index],
                               FocusChangeReason::kFocusTraversal);
      return false;
    }
  }

  // Process keyboard accelerators.
  // If the key combination matches an accelerator, the accelerator is
  // triggered, otherwise the key event is processed as usual.
  if (ProcessAccelerator(accelerator)) {
    // If a shortcut was activated for this keydown message, do not propagate
    // the event further.
    return false;
  }
  return true;
}

// Tests whether a view is valid, whether it still belongs to the window
// hierarchy of the FocusManager.
bool FocusManager::ContainsView(View* view) {
  Widget* widget = view->GetWidget();
  return widget && widget->GetFocusManager() == this;
}

void FocusManager::AdvanceFocus(bool reverse) {
  View* v = GetNextFocusableView(focused_view_, nullptr, reverse, false);
  // Note: Do not skip this next block when v == focused_view_.  If the user
  // tabs past the last focusable element in a webpage, we'll get here, and if
  // the TabContentsContainerView is the only focusable view (possible in
  // fullscreen mode), we need to run this block in order to cycle around to the
  // first element on the page.
  if (v) {
    views::View* focused_view = focused_view_;
    v->AboutToRequestFocusFromTabTraversal(reverse);
    // AboutToRequestFocusFromTabTraversal() may have changed focus. If it did,
    // don't change focus again.
    if (focused_view != focused_view_)
      return;

    // Note that GetNextFocusableView may have returned a View in a different
    // FocusManager.
    DCHECK(v->GetWidget());
    v->GetWidget()->GetFocusManager()->SetFocusedViewWithReason(
        v, FocusChangeReason::kFocusTraversal);

    // When moving focus from a child widget to a top-level widget,
    // the top-level widget may report IsActive()==true because it's
    // active even though it isn't focused. Explicitly activate the
    // widget to ensure that case is handled.
    if (v->GetWidget()->GetFocusManager() != this)
      v->GetWidget()->Activate();
  }
}

void FocusManager::ClearNativeFocus() {
  // Keep the top root window focused so we get keyboard events.
  widget_->ClearNativeFocus();
}

bool FocusManager::RotatePaneFocus(Direction direction,
                                   FocusCycleWrappingBehavior wrap) {
  // Get the list of all accessible panes.
  std::vector<View*> panes;
  widget_->widget_delegate()->GetAccessiblePanes(&panes);

  // Count the number of panes and set the default index if no pane
  // is initially focused.
  if (panes.empty())
    return false;
  int count = int{panes.size()};

  // Initialize |index| to an appropriate starting index if nothing is
  // focused initially.
  int index = direction == kBackward ? 0 : count - 1;

  // Check to see if a pane already has focus and update the index accordingly.
  const views::View* focused_view = GetFocusedView();
  if (focused_view) {
    const auto i = std::find_if(panes.cbegin(), panes.cend(),
                                [focused_view](const auto* pane) {
                                  return pane && pane->Contains(focused_view);
                                });
    if (i != panes.cend())
      index = i - panes.cbegin();
  }

  // Rotate focus.
  int start_index = index;
  for (;;) {
    if (direction == kBackward)
      index--;
    else
      index++;

    if (wrap == kNoWrap && (index >= count || index < 0))
      return false;
    index = (index + count) % count;

    // Ensure that we don't loop more than once.
    if (index == start_index)
      break;

    views::View* pane = panes[index];
    DCHECK(pane);

    if (!pane->GetVisible())
      continue;

    pane->RequestFocus();
    focused_view = GetFocusedView();
    if (pane == focused_view || pane->Contains(focused_view))
      return true;
  }

  return false;
}

View* FocusManager::GetNextFocusableView(View* original_starting_view,
                                         Widget* starting_widget,
                                         bool reverse,
                                         bool dont_loop) {
  DCHECK(!focused_view_ || ContainsView(focused_view_))
      << " focus_view=" << focused_view_;

  FocusTraversable* focus_traversable = nullptr;

  View* starting_view = nullptr;
  if (original_starting_view) {
    // Search up the containment hierarchy to see if a view is acting as
    // a pane, and wants to implement its own focus traversable to keep
    // the focus trapped within that pane.
    View* pane_search = original_starting_view;
    while (pane_search) {
      focus_traversable = pane_search->GetPaneFocusTraversable();
      if (focus_traversable) {
        starting_view = original_starting_view;
        break;
      }
      pane_search = pane_search->parent();
    }

    if (!focus_traversable) {
      if (!reverse) {
        // If the starting view has a focus traversable, use it.
        // This is the case with NativeWidgetWins for example.
        focus_traversable = original_starting_view->GetFocusTraversable();

        // Otherwise default to the root view.
        if (!focus_traversable) {
          focus_traversable =
              original_starting_view->GetWidget()->GetFocusTraversable();
          starting_view = original_starting_view;
        }
      } else {
        // When you are going back, starting view's FocusTraversable
        // should not be used.
        focus_traversable =
            original_starting_view->GetWidget()->GetFocusTraversable();
        starting_view = original_starting_view;
      }
    }
  } else {
    Widget* widget = starting_widget ? starting_widget : widget_;
    focus_traversable = widget->GetFocusTraversable();
  }

  // Traverse the FocusTraversable tree down to find the focusable view.
  View* v = FindFocusableView(focus_traversable, starting_view, reverse);
  if (v)
    return v;

  // Let's go up in the FocusTraversable tree.
  FocusTraversable* parent_focus_traversable =
      focus_traversable->GetFocusTraversableParent();
  starting_view = focus_traversable->GetFocusTraversableParentView();
  while (parent_focus_traversable) {
    FocusTraversable* new_focus_traversable = nullptr;
    View* new_starting_view = nullptr;
    // When we are going backward, the parent view might gain the next focus.
    auto check_starting_view =
        reverse ? FocusSearch::StartingViewPolicy::kCheckStartingView
                : FocusSearch::StartingViewPolicy::kSkipStartingView;
    v = parent_focus_traversable->GetFocusSearch()->FindNextFocusableView(
        starting_view,
        reverse ? FocusSearch::SearchDirection::kBackwards
                : FocusSearch::SearchDirection::kForwards,
        FocusSearch::TraversalDirection::kUp, check_starting_view,
        FocusSearch::AnchoredDialogPolicy::kSkipAnchoredDialog,
        &new_focus_traversable, &new_starting_view);

    if (new_focus_traversable) {
      DCHECK(!v);

      // There is a FocusTraversable, traverse it down.
      v = FindFocusableView(new_focus_traversable, nullptr, reverse);
    }

    if (v)
      return v;

    starting_view = focus_traversable->GetFocusTraversableParentView();
    parent_focus_traversable =
        parent_focus_traversable->GetFocusTraversableParent();
  }

  // If we get here, we have reached the end of the focus hierarchy, let's
  // loop. Make sure there was at least a view to start with, to prevent
  // infinitely looping in empty windows.
  if (dont_loop || !original_starting_view)
    return nullptr;

  // Easy, just clear the selection and press tab again.
  // By calling with nullptr as the starting view, we'll start from either
  // the starting views widget or |widget_|.
  Widget* widget = starting_view ? starting_view->GetWidget()
                                 : original_starting_view->GetWidget();
  if (widget->widget_delegate()->ShouldAdvanceFocusToTopLevelWidget())
    widget = widget_;
  return GetNextFocusableView(nullptr, widget, reverse, true);
}

void FocusManager::SetKeyboardAccessible(bool keyboard_accessible) {
  if (keyboard_accessible == keyboard_accessible_)
    return;

  keyboard_accessible_ = keyboard_accessible;
  // Disabling keyboard accessibility may cause the focused view to become not
  // focusable. Hence advance focus if necessary.
  AdvanceFocusIfNecessary();
}

void FocusManager::SetFocusedViewWithReason(View* view,
                                            FocusChangeReason reason) {
  if (focused_view_ == view)
    return;

  // TODO(oshima|achuith): This is to diagnose crbug.com/687232.
  // Change this to DCHECK once it's resolved.
  CHECK(!view || ContainsView(view));

#if !defined(OS_MACOSX)
  // TODO(warx): There are some AccessiblePaneViewTest failed on macosx.
  // crbug.com/650859. Remove !defined(OS_MACOSX) once that is fixed.
  //
  // If the widget isn't active store the focused view and then attempt to
  // activate the widget. If activation succeeds |view| will be focused.
  // If activation fails |view| will be focused the next time the widget is
  // made active.
  if (view && !widget_->IsActive()) {
    SetStoredFocusView(view);
    widget_->Activate();
    return;
  }
#endif

  // Update the reason for the focus change (since this is checked by
  // some listeners), then notify all listeners.
  focus_change_reason_ = reason;
  for (FocusChangeListener& observer : focus_change_listeners_)
    observer.OnWillChangeFocus(focused_view_, view);

  View* old_focused_view = focused_view_;
  focused_view_ = view;
  if (old_focused_view) {
    old_focused_view->RemoveObserver(this);
    old_focused_view->Blur();
  }
  // Also make |focused_view_| the stored focus view. This way the stored focus
  // view is remembered if focus changes are requested prior to a show or while
  // hidden.
  SetStoredFocusView(focused_view_);
  if (focused_view_) {
    focused_view_->AddObserver(this);
    focused_view_->Focus();
  }

  for (FocusChangeListener& observer : focus_change_listeners_)
    observer.OnDidChangeFocus(old_focused_view, focused_view_);

  if (delegate_)
    delegate_->OnDidChangeFocus(old_focused_view, focused_view_);
}

void FocusManager::SetFocusedView(View* view) {
  FocusChangeReason reason = FocusChangeReason::kDirectFocusChange;
  if (in_restoring_focused_view_)
    reason = FocusChangeReason::kFocusRestore;

  SetFocusedViewWithReason(view, reason);
}

void FocusManager::ClearFocus() {
  // SetFocusedView(nullptr) is going to clear out the stored view to. We need
  // to persist it in this case.
  views::View* focused_view = GetStoredFocusView();
  SetFocusedView(nullptr);
  ClearNativeFocus();
  SetStoredFocusView(focused_view);
}

void FocusManager::AdvanceFocusIfNecessary() {
  // If widget is inactive, there is no focused view to check. The stored view
  // will also be checked for focusability when it is being restored.
  if (!widget_->IsActive())
    return;

  // If widget is active and focused view is not focusable, advance focus or,
  // if not possible, clear focus.
  if (focused_view_ && !IsFocusable(focused_view_)) {
    AdvanceFocus(false);
    if (focused_view_ && !IsFocusable(focused_view_))
      ClearFocus();
  }
}

void FocusManager::StoreFocusedView(bool clear_native_focus) {
  View* focused_view = focused_view_;
  // Don't do anything if no focused view. Storing the view (which is nullptr),
  // in this case, would clobber the view that was previously saved.
  if (!focused_view_)
    return;

  View* v = focused_view_;

  if (clear_native_focus) {
    // Temporarily disable notification.  ClearFocus() will set the focus to the
    // main browser window.  This extra focus bounce which happens during
    // deactivation can confuse registered WidgetFocusListeners, as the focus
    // is not changing due to a user-initiated event.
    AutoNativeNotificationDisabler local_notification_disabler;
    // ClearFocus() also stores the focused view.
    ClearFocus();
  } else {
    SetFocusedView(nullptr);
    SetStoredFocusView(focused_view);
  }

  if (v)
    v->SchedulePaint();  // Remove focus border.
}

bool FocusManager::RestoreFocusedView() {
  View* view = GetStoredFocusView();
  if (view) {
    if (ContainsView(view)) {
      if (!view->IsFocusable() && view->IsAccessibilityFocusable()) {
        // RequestFocus would fail, but we want to restore focus to controls
        // that had focus in accessibility mode.
        SetFocusedViewWithReason(view, FocusChangeReason::kFocusRestore);
      } else {
        // This usually just sets the focus if this view is focusable, but
        // let the view override RequestFocus if necessary.
        base::AutoReset<bool> in_restore_bit(&in_restoring_focused_view_, true);
        view->RequestFocus();
      }
    }
    // The |keyboard_accessible_| mode may have changed while the widget was
    // inactive.
    AdvanceFocusIfNecessary();
  }
  return view && view == focused_view_;
}

void FocusManager::SetStoredFocusView(View* focus_view) {
  view_tracker_for_stored_view_->SetView(focus_view);
}

View* FocusManager::GetStoredFocusView() {
  return view_tracker_for_stored_view_->view();
}

// Find the next (previous if reverse is true) focusable view for the specified
// FocusTraversable, starting at the specified view, traversing down the
// FocusTraversable hierarchy.
View* FocusManager::FindFocusableView(FocusTraversable* focus_traversable,
                                      View* starting_view,
                                      bool reverse) {
  FocusTraversable* new_focus_traversable = nullptr;
  View* new_starting_view = nullptr;
  auto can_go_into_anchored_dialog =
      FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog;
  View* v = focus_traversable->GetFocusSearch()->FindNextFocusableView(
      starting_view,
      reverse ? FocusSearch::SearchDirection::kBackwards
              : FocusSearch::SearchDirection::kForwards,
      FocusSearch::TraversalDirection::kDown,
      FocusSearch::StartingViewPolicy::kSkipStartingView,
      can_go_into_anchored_dialog, &new_focus_traversable, &new_starting_view);

  // Let's go down the FocusTraversable tree as much as we can.
  while (new_focus_traversable) {
    DCHECK(!v);
    focus_traversable = new_focus_traversable;
    new_focus_traversable = nullptr;
    starting_view = nullptr;
    v = focus_traversable->GetFocusSearch()->FindNextFocusableView(
        starting_view,
        reverse ? FocusSearch::SearchDirection::kBackwards
                : FocusSearch::SearchDirection::kForwards,
        FocusSearch::TraversalDirection::kDown,
        FocusSearch::StartingViewPolicy::kSkipStartingView,
        can_go_into_anchored_dialog, &new_focus_traversable,
        &new_starting_view);
  }
  return v;
}

void FocusManager::RegisterAccelerator(
    const ui::Accelerator& accelerator,
    ui::AcceleratorManager::HandlerPriority priority,
    ui::AcceleratorTarget* target) {
  accelerator_manager_.Register({accelerator}, priority, target);
}

void FocusManager::UnregisterAccelerator(const ui::Accelerator& accelerator,
                                         ui::AcceleratorTarget* target) {
  accelerator_manager_.Unregister(accelerator, target);
}

void FocusManager::UnregisterAccelerators(ui::AcceleratorTarget* target) {
  accelerator_manager_.UnregisterAll(target);
}

bool FocusManager::ProcessAccelerator(const ui::Accelerator& accelerator) {
  if (accelerator_manager_.Process(accelerator))
    return true;
  return delegate_ && delegate_->ProcessAccelerator(accelerator);
}

bool FocusManager::HasPriorityHandler(
    const ui::Accelerator& accelerator) const {
  return accelerator_manager_.HasPriorityHandler(accelerator);
}

// static
bool FocusManager::IsTabTraversalKeyEvent(const ui::KeyEvent& key_event) {
  return key_event.key_code() == ui::VKEY_TAB &&
         (!key_event.IsControlDown() && !key_event.IsAltDown());
}

void FocusManager::ViewRemoved(View* removed) {
  // If the view being removed contains (or is) the focused view,
  // clear the focus.  However, it's not safe to call ClearFocus()
  // (and in turn ClearNativeFocus()) here because ViewRemoved() can
  // be called while the top level widget is being destroyed.
  DCHECK(removed);
  if (removed->Contains(focused_view_))
    SetFocusedView(nullptr);
}

void FocusManager::AddFocusChangeListener(FocusChangeListener* listener) {
  focus_change_listeners_.AddObserver(listener);
}

void FocusManager::RemoveFocusChangeListener(FocusChangeListener* listener) {
  focus_change_listeners_.RemoveObserver(listener);
}

bool FocusManager::ProcessArrowKeyTraversal(const ui::KeyEvent& event) {
  if (event.IsShiftDown() || event.IsControlDown() || event.IsAltDown())
    return false;

  const ui::KeyboardCode key = event.key_code();
  if (key != ui::VKEY_UP && key != ui::VKEY_DOWN && key != ui::VKEY_LEFT &&
      key != ui::VKEY_RIGHT) {
    return false;
  }

  const ui::KeyboardCode reverse =
      base::i18n::IsRTL() ? ui::VKEY_RIGHT : ui::VKEY_LEFT;
  AdvanceFocus(key == reverse || key == ui::VKEY_UP);
  return true;
}

bool FocusManager::IsFocusable(View* view) const {
  DCHECK(view);

// |keyboard_accessible_| is only used on Mac.
#if defined(OS_MACOSX)
  return keyboard_accessible_ ? view->IsAccessibilityFocusable()
                              : view->IsFocusable();
#else
  return view->IsAccessibilityFocusable();
#endif
}

void FocusManager::OnViewIsDeleting(View* view) {
  // Typically ViewRemoved() is called and all the cleanup happens there. With
  // child widgets it's possible to change the parent out from under the Widget
  // such that ViewRemoved() is never called.
  CHECK_EQ(view, focused_view_);
  SetFocusedView(nullptr);
}

}  // namespace views
