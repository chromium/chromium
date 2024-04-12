// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/focus_search.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

FocusSearch::FocusSearch(View* root, bool cycle, bool accessibility_mode)
    : root_(root), cycle_(cycle), accessibility_mode_(accessibility_mode) {
#if BUILDFLAG(IS_MAC)
  // On Mac, only the keyboard accessibility mode defined in FocusManager is
  // used. No special accessibility mode should be applicable for a
  // FocusTraversable.
  accessibility_mode_ = false;
#endif
}

View* FocusSearch::FindNextFocusableView(
    View* starting_view,
    FocusSearch::SearchDirection search_direction,
    FocusSearch::TraversalDirection traversal_direction,
    FocusSearch::StartingViewPolicy check_starting_view,
    FocusSearch::AnchoredDialogPolicy can_go_into_anchored_dialog,
    FocusTraversable** focus_traversable,
    View** focus_traversable_view) {
  DCHECK(!root_->children().empty());

  *focus_traversable = nullptr;
  *focus_traversable_view = nullptr;

  View* initial_starting_view = starting_view;
  int starting_view_group = -1;
  if (starting_view)
    starting_view_group = starting_view->GetGroup();

  if (!starting_view) {
    // Default to the first/last child
    starting_view = search_direction == SearchDirection::kBackwards
                        ? root_->children().back()
                        : root_->children().front();
    // If there was no starting view, then the one we select is a potential
    // focus candidate.
    check_starting_view = StartingViewPolicy::kCheckStartingView;
  } else {
    // The starting view should be a direct or indirect child of the root.
    DCHECK(Contains(root_, starting_view));
  }

  base::flat_set<View*> seen_views;
  View* v = nullptr;
  if (search_direction == SearchDirection::kForwards) {
    v = FindNextFocusableViewImpl(
        starting_view, check_starting_view, true,
        (traversal_direction == TraversalDirection::kDown),
        can_go_into_anchored_dialog, starting_view_group, &seen_views,
        focus_traversable, focus_traversable_view);
  } else {
    // If the starting view is focusable, we don't want to go down, as we are
    // traversing the view hierarchy tree bottom-up.
    bool can_go_down = (traversal_direction == TraversalDirection::kDown) &&
                       !IsFocusable(starting_view);
    v = FindPreviousFocusableViewImpl(
        starting_view, check_starting_view, true, can_go_down,
        can_go_into_anchored_dialog, starting_view_group, &seen_views,
        focus_traversable, focus_traversable_view);
  }

  // Don't set the focus to something outside of this view hierarchy.
  if (v && v != root_ && !Contains(root_, v))
    v = nullptr;

  // If we should go into a sub-FocusTraversable (such as an anchored bubble), a
  // null View is returned and |focus_traversable| is set appropriately. Handle
  // this case before cycling. Note that Find{Next,Previous}FocusableViewImpl
  // respect |can_go_into_anchored_dialog| so we don't need to check it here.
  if (*focus_traversable) {
    DCHECK(*focus_traversable_view);
    DCHECK_EQ(v, nullptr);
    return nullptr;
  }

  // If |cycle_| is true, prefer to keep cycling rather than returning nullptr.
  if (cycle_ && !v && initial_starting_view) {
    v = FindNextFocusableView(nullptr, search_direction, traversal_direction,
                              check_starting_view, can_go_into_anchored_dialog,
                              focus_traversable, focus_traversable_view);
  }

  // Doing some sanity checks.
  if (v) {
    DCHECK(IsFocusable(v));
    return v;
  }

  // Nothing found.
  return nullptr;
}

bool FocusSearch::IsViewFocusableCandidate(View* v, int skip_group_id) {
  return IsFocusable(v) &&
         (v->IsGroupFocusTraversable() || skip_group_id == -1 ||
          v->GetGroup() != skip_group_id);
}

bool FocusSearch::IsFocusable(View* v) {
  DCHECK(root_);
  // Sanity Check. Currently the FocusManager keyboard accessibility mode is
  // only used on Mac, for which |accessibility_mode_| is false.
  DCHECK(!(accessibility_mode_ &&
           root_->GetWidget()->GetFocusManager()->keyboard_accessible()));
  if (accessibility_mode_ ||
      root_->GetWidget()->GetFocusManager()->keyboard_accessible())
    return v && v->GetViewAccessibility().IsAccessibilityFocusable();
  return v && v->IsFocusable();
}

View* FocusSearch::FindSelectedViewForGroup(View* view) {
  if (view->IsGroupFocusTraversable() ||
      view->GetGroup() == -1)  // No group for that view.
    return view;

  View* selected_view = view->GetSelectedViewForGroup(view->GetGroup());
  if (selected_view)
    return selected_view;

  // No view selected for that group, default to the specified view.
  return view;
}

View* FocusSearch::GetParent(View* v) {
  return Contains(root_, v) ? v->parent() : nullptr;
}

bool FocusSearch::Contains(View* root, const View* v) {
  return root->Contains(v);
}

// Strategy for finding the next focusable view:
// - keep going down the first child, stop when you find a focusable view or
//   a focus traversable view (in that case return it) or when you reach a view
//   with no children.
// - go to the right sibling and start the search from there (by invoking
//   FindNextFocusableViewImpl on that view).
// - if the view has no right sibling, go up the parents until you find a parent
//   with a right sibling and start the search from there.
View* FocusSearch::FindNextFocusableViewImpl(
    View* starting_view,
    FocusSearch::StartingViewPolicy check_starting_view,
    bool can_go_up,
    bool can_go_down,
    AnchoredDialogPolicy can_go_into_anchored_dialog,
    int skip_group_id,
    base::flat_set<View*>* seen_views,
    FocusTraversable** focus_traversable,
    View** focus_traversable_view) {
  // Views are not supposed to have focus cycles, but just in case, fail
  // gracefully to avoid a crash.
  if (seen_views->contains(starting_view)) {
    LOG(ERROR) << "View focus cycle detected.";
    return nullptr;
  }
  seen_views->insert(starting_view);

  if (check_starting_view == StartingViewPolicy::kCheckStartingView) {
    if (IsViewFocusableCandidate(starting_view, skip_group_id)) {
      View* v = FindSelectedViewForGroup(starting_view);
      // The selected view might not be focusable (if it is disabled for
      // example).
      if (IsFocusable(v))
        return v;
    }

    *focus_traversable = starting_view->GetFocusTraversable();
    if (*focus_traversable) {
      *focus_traversable_view = starting_view;
      return nullptr;
    }
  }

  // First let's try the left child.
  if (can_go_down) {
    if (!starting_view->children().empty()) {
      // This view might not be `IsFocusable` but the view is still passed
      // down to evaluate if one of it's children `IsFocusable`.
      View* view = starting_view->GetChildrenFocusList().front();
      View* v = FindNextFocusableViewImpl(
          view, StartingViewPolicy::kCheckStartingView, false, true,
          can_go_into_anchored_dialog, skip_group_id, seen_views,
          focus_traversable, focus_traversable_view);
      if (v || *focus_traversable)
        return v;
    }

    // Check to see if we should navigate into a dialog anchored at this view.
    if (can_go_into_anchored_dialog ==
        AnchoredDialogPolicy::kCanGoIntoAnchoredDialog) {
      DialogDelegate* bubble = starting_view->GetProperty(kAnchoredDialogKey);
      if (bubble) {
        *focus_traversable = bubble->GetWidget()->GetFocusTraversable();
        *focus_traversable_view = starting_view;
        return nullptr;
      }
    }
  }

  // Then try the right sibling.
  View* sibling = starting_view->GetNextFocusableView();
  if (sibling) {
    View* v = FindNextFocusableViewImpl(
        sibling, FocusSearch::StartingViewPolicy::kCheckStartingView, false,
        true, can_go_into_anchored_dialog, skip_group_id, seen_views,
        focus_traversable, focus_traversable_view);
    if (v || *focus_traversable)
      return v;
  }

  // Then go up to the parent sibling.
  if (can_go_up) {
    View* parent = GetParent(starting_view);
    while (parent && parent != root_) {
      if (can_go_into_anchored_dialog ==
          AnchoredDialogPolicy::kCanGoIntoAnchoredDialog) {
        DialogDelegate* bubble = parent->GetProperty(kAnchoredDialogKey);
        if (bubble) {
          *focus_traversable = bubble->GetWidget()->GetFocusTraversable();
          *focus_traversable_view = starting_view;
          return nullptr;
        }
      }

      sibling = parent->GetNextFocusableView();
      if (sibling) {
        return FindNextFocusableViewImpl(
            sibling, StartingViewPolicy::kCheckStartingView, true, true,
            can_go_into_anchored_dialog, skip_group_id, seen_views,
            focus_traversable, focus_traversable_view);
      }
      parent = GetParent(parent);
    }
  }

  // We found nothing.
  return nullptr;
}

// Strategy for finding the previous focusable view:
// - keep going down on the right until you reach a view with no children, if it
//   it is a good candidate return it.
// - start the search on the left sibling.
// - if there are no left sibling, start the search on the parent (without going
//   down).
View* FocusSearch::FindPreviousFocusableViewImpl(
    View* starting_view,
    FocusSearch::StartingViewPolicy check_starting_view,
    bool can_go_up,
    bool can_go_down,
    FocusSearch::AnchoredDialogPolicy can_go_into_anchored_dialog,
    int skip_group_id,
    base::flat_set<View*>* seen_views,
    FocusTraversable** focus_traversable,
    View** focus_traversable_view) {
  // Views are not supposed to have focus cycles, but just in case, fail
  // gracefully to avoid a crash.
  if (seen_views->contains(starting_view)) {
    LOG(ERROR) << "View focus cycle detected.";
    return nullptr;
  }
  seen_views->insert(starting_view);

  // Normally when we navigate to a FocusTraversableParent, can_go_down is
  // false so we don't navigate back in. However, if we just navigated out
  // of an anchored dialog, allow going down in order to navigate into
  // children of |starting_view| next.
  if (starting_view->GetProperty(kAnchoredDialogKey) &&
      can_go_into_anchored_dialog ==
          AnchoredDialogPolicy::kSkipAnchoredDialog &&
      !can_go_down) {
    can_go_down = true;
  }

  // Let's go down and right as much as we can.
  if (can_go_down) {
    // Before we go into the direct children, we have to check if this view has
    // a FocusTraversable.
    *focus_traversable = starting_view->GetFocusTraversable();
    if (*focus_traversable) {
      *focus_traversable_view = starting_view;
      return nullptr;
    }

    // Check to see if we should navigate into a dialog anchored at this view.
    if (can_go_into_anchored_dialog ==
        AnchoredDialogPolicy::kCanGoIntoAnchoredDialog) {
      DialogDelegate* bubble = starting_view->GetProperty(kAnchoredDialogKey);
      if (bubble) {
        *focus_traversable = bubble->GetWidget()->GetFocusTraversable();
        *focus_traversable_view = starting_view;
        return nullptr;
      }
    }

    can_go_into_anchored_dialog =
        AnchoredDialogPolicy::kCanGoIntoAnchoredDialog;
    if (!starting_view->children().empty()) {
      // This view might not be `IsFocusable` but the view is still passed
      // down to evaluate if one of it's children `IsFocusable`.
      View* view = starting_view->GetChildrenFocusList().back();
      View* v = FindPreviousFocusableViewImpl(
          view, StartingViewPolicy::kCheckStartingView, false, true,
          can_go_into_anchored_dialog, skip_group_id, seen_views,
          focus_traversable, focus_traversable_view);
      if (v || *focus_traversable)
        return v;
    }
  }

  // Then look at this view. Here, we do not need to see if the view has
  // a FocusTraversable, since we do not want to go down any more.
  if (check_starting_view == StartingViewPolicy::kCheckStartingView &&
      IsViewFocusableCandidate(starting_view, skip_group_id)) {
    View* v = FindSelectedViewForGroup(starting_view);
    // The selected view might not be focusable (if it is disabled for
    // example).
    if (IsFocusable(v))
      return v;
  }

  // Then try the left sibling.
  View* sibling = starting_view->GetPreviousFocusableView();
  if (sibling) {
    return FindPreviousFocusableViewImpl(
        sibling, StartingViewPolicy::kCheckStartingView, can_go_up, true,
        can_go_into_anchored_dialog, skip_group_id, seen_views,
        focus_traversable, focus_traversable_view);
  }

  // Then go up the parent.
  if (can_go_up) {
    View* parent = GetParent(starting_view);
    if (parent)
      return FindPreviousFocusableViewImpl(
          parent, StartingViewPolicy::kCheckStartingView, true, false,
          can_go_into_anchored_dialog, skip_group_id, seen_views,
          focus_traversable, focus_traversable_view);
  }

  // We found nothing.
  return nullptr;
}

}  // namespace views
