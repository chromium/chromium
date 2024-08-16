// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessible_pane_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace views {

// Create tiny subclass of FocusSearch that overrides GetParent and Contains,
// delegating these to methods in AccessiblePaneView. This is needed so that
// subclasses of AccessiblePaneView can customize the focus search logic and
// include views that aren't part of the AccessiblePaneView's view
// hierarchy in the focus order.
class AccessiblePaneViewFocusSearch : public FocusSearch {
 public:
  explicit AccessiblePaneViewFocusSearch(AccessiblePaneView* pane_view)
      : FocusSearch(pane_view, true, true), accessible_pane_view_(pane_view) {}

  AccessiblePaneViewFocusSearch(const AccessiblePaneViewFocusSearch&) = delete;
  AccessiblePaneViewFocusSearch& operator=(
      const AccessiblePaneViewFocusSearch&) = delete;

 protected:
  View* GetParent(View* v) override {
    return accessible_pane_view_->ContainsForFocusSearch(root(), v)
               ? accessible_pane_view_->GetParentForFocusSearch(v)
               : nullptr;
  }

  // Returns true if |v| is contained within the hierarchy rooted at |root|.
  // Subclasses can override this if they need custom focus search behavior.
  bool Contains(View* root, const View* v) override {
    return accessible_pane_view_->ContainsForFocusSearch(root, v);
  }

 private:
  raw_ptr<AccessiblePaneView> accessible_pane_view_;
};

AccessiblePaneView::AccessiblePaneView()
    : last_focused_view_tracker_(std::make_unique<ViewTracker>()) {
  focus_search_ = std::make_unique<AccessiblePaneViewFocusSearch>(this);
  GetViewAccessibility().SetRole(ax::mojom::Role::kPane);
}

AccessiblePaneView::~AccessiblePaneView() {
  if (pane_has_focus_) {
    RemovePaneFocus();
  }
}

bool AccessiblePaneView::SetPaneFocus(views::View* initial_focus) {
  if (!GetVisible())
    return false;

  if (!focus_manager_)
    focus_manager_ = GetFocusManager();

  View* focused_view = focus_manager_->GetFocusedView();
  if (focused_view && !ContainsForFocusSearch(this, focused_view))
    last_focused_view_tracker_->SetView(focused_view);

  // Use the provided initial focus if it's visible and enabled, otherwise
  // use the first focusable child.
  if (!initial_focus || !ContainsForFocusSearch(this, initial_focus) ||
      !initial_focus->GetVisible() || !initial_focus->GetEnabled()) {
    initial_focus = GetFirstFocusableChild();
  }

  // Return false if there are no focusable children.
  if (!initial_focus)
    return false;

  focus_manager_->SetFocusedView(initial_focus);

  // TODO(pbos): Move this behavior into FocusManager. Focusing an unfocusable
  // view should do something smart (move focus to its children or clear focus).
  // DownloadItemView is an example (isn't focusable, has focusable children).
  // See https://crbug.com/1000998.
  // The initially-focused view may not be focusable (but one of its children
  // might). We may need to advance focus here to make sure focus is on
  // something focusable.
  focus_manager_->AdvanceFocusIfNecessary();

  // If we already have pane focus, we're done.
  if (pane_has_focus_)
    return true;

  // Otherwise, set accelerators and start listening for focus change events.
  pane_has_focus_ = true;
  ui::AcceleratorManager::HandlerPriority normal =
      ui::AcceleratorManager::kNormalPriority;
  focus_manager_->RegisterAccelerator(home_key_, normal, this);
  focus_manager_->RegisterAccelerator(end_key_, normal, this);
  focus_manager_->RegisterAccelerator(escape_key_, normal, this);
  focus_manager_->RegisterAccelerator(left_key_, normal, this);
  focus_manager_->RegisterAccelerator(right_key_, normal, this);
  focus_manager_->AddFocusChangeListener(this);

  return true;
}

bool AccessiblePaneView::SetPaneFocusAndFocusDefault() {
  return SetPaneFocus(GetDefaultFocusableChild());
}

views::View* AccessiblePaneView::GetDefaultFocusableChild() {
  return nullptr;
}

View* AccessiblePaneView::GetParentForFocusSearch(View* v) {
  return v->parent();
}

bool AccessiblePaneView::ContainsForFocusSearch(View* root, const View* v) {
  return root->Contains(v);
}

void AccessiblePaneView::RemovePaneFocus() {
  focus_manager_->RemoveFocusChangeListener(this);
  pane_has_focus_ = false;

  focus_manager_->UnregisterAccelerator(home_key_, this);
  focus_manager_->UnregisterAccelerator(end_key_, this);
  focus_manager_->UnregisterAccelerator(escape_key_, this);
  focus_manager_->UnregisterAccelerator(left_key_, this);
  focus_manager_->UnregisterAccelerator(right_key_, this);
}

views::View* AccessiblePaneView::GetFirstFocusableChild() {
  FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  return focus_search_->FindNextFocusableView(
      nullptr, FocusSearch::SearchDirection::kForwards,
      FocusSearch::TraversalDirection::kDown,
      FocusSearch::StartingViewPolicy::kSkipStartingView,
      FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
}

views::View* AccessiblePaneView::GetLastFocusableChild() {
  FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  return focus_search_->FindNextFocusableView(
      this, FocusSearch::SearchDirection::kBackwards,
      FocusSearch::TraversalDirection::kDown,
      FocusSearch::StartingViewPolicy::kSkipStartingView,
      FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
}

////////////////////////////////////////////////////////////////////////////////
// View overrides:

views::FocusTraversable* AccessiblePaneView::GetPaneFocusTraversable() {
  if (pane_has_focus_)
    return this;
  else
    return nullptr;
}

bool AccessiblePaneView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  views::View* focused_view = focus_manager_->GetFocusedView();
  if (!ContainsForFocusSearch(this, focused_view))
    return false;

  using FocusChangeReason = views::FocusManager::FocusChangeReason;
  switch (accelerator.key_code()) {
    case ui::VKEY_ESCAPE: {
      RemovePaneFocus();
      View* last_focused_view = last_focused_view_tracker_->view();
      // Ignore |last_focused_view| if it's no longer in the same widget.
      if (last_focused_view && GetWidget() != last_focused_view->GetWidget())
        last_focused_view = nullptr;
      if (last_focused_view) {
        focus_manager_->SetFocusedViewWithReason(
            last_focused_view, FocusChangeReason::kFocusRestore);
      } else if (allow_deactivate_on_esc_) {
        focused_view->GetWidget()->Deactivate();
      }
      return true;
    }
    case ui::VKEY_LEFT:
      focus_manager_->AdvanceFocus(true);
      return true;
    case ui::VKEY_RIGHT:
      focus_manager_->AdvanceFocus(false);
      return true;
    case ui::VKEY_HOME:
      focus_manager_->SetFocusedViewWithReason(
          GetFirstFocusableChild(), FocusChangeReason::kFocusTraversal);
      return true;
    case ui::VKEY_END:
      focus_manager_->SetFocusedViewWithReason(
          GetLastFocusableChild(), FocusChangeReason::kFocusTraversal);
      return true;
    default:
      return false;
  }
}

void AccessiblePaneView::SetVisible(bool flag) {
  if (GetVisible() && !flag && pane_has_focus_) {
    RemovePaneFocus();
    focus_manager_->RestoreFocusedView();
  }
  View::SetVisible(flag);
}

void AccessiblePaneView::RequestFocus() {
  SetPaneFocusAndFocusDefault();
}

////////////////////////////////////////////////////////////////////////////////
// FocusChangeListener overrides:

void AccessiblePaneView::OnWillChangeFocus(views::View* focused_before,
                                           views::View* focused_now) {
  //  Act when focus has changed.
}

void AccessiblePaneView::OnDidChangeFocus(views::View* focused_before,
                                          views::View* focused_now) {
  if (!focused_now)
    return;

  views::FocusManager::FocusChangeReason reason =
      focus_manager_->focus_change_reason();

  if (!ContainsForFocusSearch(this, focused_now) ||
      reason == views::FocusManager::FocusChangeReason::kDirectFocusChange) {
    // We should remove pane focus (i.e. make most of the controls
    // not focusable again) because the focus has left the pane,
    // or because the focus changed within the pane due to the user
    // directly focusing to a specific view (e.g., clicking on it).
    RemovePaneFocus();
  }
}

////////////////////////////////////////////////////////////////////////////////
// FocusTraversable overrides:

views::FocusSearch* AccessiblePaneView::GetFocusSearch() {
  DCHECK(pane_has_focus_);
  return focus_search_.get();
}

views::FocusTraversable* AccessiblePaneView::GetFocusTraversableParent() {
  DCHECK(pane_has_focus_);
  return nullptr;
}

views::View* AccessiblePaneView::GetFocusTraversableParentView() {
  DCHECK(pane_has_focus_);
  return nullptr;
}

BEGIN_METADATA(AccessiblePaneView)
END_METADATA

}  // namespace views
