// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/focus_manager.h"

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
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
  const ui::KeyboardCode key_code = event.key_code();

  if (event.type() != ui::EventType::kKeyPressed &&
      event.type() != ui::EventType::kKeyReleased) {
    return false;
  }

  if (shortcut_handling_suspended())
    return true;

  ui::Accelerator accelerator(event);

    // If the focused view wants to process the key event as is, let it be.
  if (focused_view_ && focused_view_->SkipDefaultKeyEventProcessing(event) &&
      !accelerator_manager_.HasPriorityHandler(accelerator)) {
    return true;
  }

  if (event.type() == ui::EventType::kKeyPressed) {
    // Intercept Tab related messages for focus traversal.
    // Note that we don't do focus traversal if the root window is not part of
    // the active window hierarchy as this would mean we have no focused view
    // and would focus the first focusable view.
    if (IsTabTraversalKeyEvent(event)) {
      AdvanceFocus(event.IsShiftDown());
      return false;
    }

    if (IsArrowKeyTraversalEnabledForWidget() &&
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
      // Remove any views except current, which are disabled or hidden.
      std::erase_if(views, [this](View* v) {
        return v != focused_view_ &&
               !v->GetViewAccessibility().IsAccessibilityFocusable();
      });
      View::Views::const_iterator i = base::ranges::find(views, focused_view_);
      DCHECK(i != views.end());
      auto index = static_cast<size_t>(i - views.begin());
      if (next && index == views.size() - 1)
        index = 0;
      else if (!next && index == 0)
        index = views.size() - 1;
      else
        index = next ? (index + 1) : (index - 1);
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
                                   FocusCycleWrapping wrapping) {
  return widget_->widget_delegate()->RotatePaneFocusFromView(
      GetFocusedView(), direction == Direction::kForward,
      wrapping == FocusCycleWrapping::kEnabled);
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
    Widget* widget = starting_widget ? starting_widget : widget_.get();
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
  if (widget->widget_delegate()->focus_traverses_out())
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

bool FocusManager::IsSettingFocusedView() const {
  return setting_focused_view_entrance_count_ > 0;
}

void FocusManager::SetFocusedViewWithReason(View* view,
                                            FocusChangeReason reason) {
  if (focused_view_ == view)
    return;

  // TODO(oshima|achuith): This is to diagnose crbug.com/687232.
  // Change this to DCHECK once it's resolved.
  CHECK(!view || ContainsView(view));

#if !BUILDFLAG(IS_MAC)
  // TODO(warx): There are some AccessiblePaneViewTest failed on macosx.
  // crbug.com/650859. Remove !BUILDFLAG(IS_MAC) once that is fixed.
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
  focus_change_listeners_.Notify(&FocusChangeListener::OnWillChangeFocus,
                                 focused_view_, view);

  View* old_focused_view = focused_view_;
  focused_view_ = view;
  base::AutoReset<int> entrance_count_resetter(
      &setting_focused_view_entrance_count_,
      setting_focused_view_entrance_count_ + 1);

  if (old_focused_view) {
    old_focused_view->RemoveObserver(this);
    old_focused_view->Blur();
  }
  // Also make |focused_view_| the stored focus view. This way the stored focus
  // view is remembered if focus changes are requested prior to a show or while
  // hidden.
  SetStoredFocusView(focused_view_);
  if (focused_view_) {
    // TODO(40763787): Remove this once reentrant callsites have been addressed.
    if (!focused_view_->HasObserver(this)) {
      focused_view_->AddObserver(this);
    }
    focused_view_->Focus();
  }

  focus_change_listeners_.Notify(&FocusChangeListener::OnDidChangeFocus,
                                 old_focused_view, focused_view_);
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
      if (!view->IsFocusable() &&
          view->GetViewAccessibility().IsAccessibilityFocusable()) {
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
  const FocusSearch::AnchoredDialogPolicy can_go_into_anchored_dialog =
      FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog;
  const FocusSearch::SearchDirection search_direction =
      reverse ? FocusSearch::SearchDirection::kBackwards
              : FocusSearch::SearchDirection::kForwards;
  View* v = nullptr;

  // Let's go down the FocusTraversable tree as much as we can.
  do {
    v = focus_traversable->GetFocusSearch()->FindNextFocusableView(
        starting_view, search_direction, FocusSearch::TraversalDirection::kDown,
        FocusSearch::StartingViewPolicy::kSkipStartingView,
        can_go_into_anchored_dialog, &new_focus_traversable,
        &new_starting_view);
    DCHECK(!new_focus_traversable || !v);
    focus_traversable = std::exchange(new_focus_traversable, nullptr);
    starting_view = nullptr;
  } while (focus_traversable);

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
  if (delegate_ && delegate_->ProcessAccelerator(accelerator))
    return true;

#if BUILDFLAG(IS_MAC)
  // On MacOS accelerators are processed when a bubble is opened without
  // manual redirection to bubble anchor widget. Including redirect on MacOS
  // breaks processing accelerators by the bubble itself.
  return false;
#else
  return RedirectAcceleratorToBubbleAnchorWidget(accelerator);
#endif
}

bool FocusManager::IsAcceleratorRegistered(
    const ui::Accelerator& accelerator) const {
  return accelerator_manager_.IsRegistered(accelerator);
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
  if (event.IsShiftDown() || event.IsControlDown() || event.IsAltDown() ||
      event.IsAltGrDown())
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
#if BUILDFLAG(IS_MAC)
  return keyboard_accessible_
             ? view->GetViewAccessibility().IsAccessibilityFocusable()
             : view->IsFocusable();
#else
  return view->GetViewAccessibility().IsAccessibilityFocusable();
#endif
}

void FocusManager::OnViewIsDeleting(View* view) {
  // Typically ViewRemoved() is called and all the cleanup happens there. With
  // child widgets it's possible to change the parent out from under the Widget
  // such that ViewRemoved() is never called.
  CHECK_EQ(view, focused_view_);
  SetFocusedView(nullptr);
}

bool FocusManager::RedirectAcceleratorToBubbleAnchorWidget(
    const ui::Accelerator& accelerator) {
  if (!widget_->widget_delegate())
    return false;

  views::BubbleDialogDelegate* widget_delegate =
      widget_->widget_delegate()->AsBubbleDialogDelegate();
  Widget* anchor_widget =
      widget_delegate ? widget_delegate->anchor_widget() : nullptr;
  if (!anchor_widget)
    return false;

  FocusManager* focus_manager = anchor_widget->GetFocusManager();
  if (!focus_manager->IsAcceleratorRegistered(accelerator))
    return false;

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Processing an accelerator can delete things. Because we
  // need these objects afterwards on Linux, save widget_ as weak pointer and
  // save the close_on_deactivate property value of widget_delegate in a
  // variable.
  base::WeakPtr<Widget> widget_weak_ptr = widget_->GetWeakPtr();
  const bool close_widget_on_deactivate =
      widget_delegate->ShouldCloseOnDeactivate();
#endif

  // The parent view must be focused for it to process events.
  focus_manager->SetFocusedView(anchor_widget->GetRootView());
  const bool accelerator_processed =
      focus_manager->ProcessAccelerator(accelerator);

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Need to manually close the bubble widget on Linux. On Linux when the
  // bubble is shown, the main widget remains active. Because of that when
  // focus is set to the main widget to process accelerator, the main widget
  // isn't activated and the bubble widget isn't deactivated and closed.
  if (accelerator_processed && close_widget_on_deactivate) {
    widget_weak_ptr->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }
#endif

  return accelerator_processed;
}

bool FocusManager::IsArrowKeyTraversalEnabledForWidget() const {
  if (arrow_key_traversal_enabled_)
    return true;

  Widget* const widget = (focused_view_ && focused_view_->GetWidget())
                             ? focused_view_->GetWidget()
                             : widget_.get();
  return widget && widget->widget_delegate() &&
         widget->widget_delegate()->enable_arrow_key_traversal();
}

}  // namespace views
