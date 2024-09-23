// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_FOCUS_FOCUS_SEARCH_H_
#define UI_VIEWS_FOCUS_FOCUS_SEARCH_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace views {

class FocusTraversable;

// FocusSearch is an object that implements the algorithm to find the
// next view to focus.
class VIEWS_EXPORT FocusSearch {
 public:
  // The direction in which the focus traversal is going.
  // TODO(jcampan): add support for lateral (left, right) focus traversal. The
  // goal is to switch to focusable views on the same level when using the arrow
  // keys (ala Windows: in a dialog box, arrow keys typically move between the
  // dialog OK, Cancel buttons).
  enum class TraversalDirection {
    kUp,
    kDown,
  };

  enum class SearchDirection {
    kForwards,
    kBackwards,
  };

  enum class StartingViewPolicy {
    kSkipStartingView,
    kCheckStartingView,
  };

  enum class AnchoredDialogPolicy {
    kSkipAnchoredDialog,
    kCanGoIntoAnchoredDialog,
  };

  // Constructor.
  // - |root| is the root of the view hierarchy to traverse. Focus will be
  //   trapped inside.
  // - |cycle| should be true if you want FindNextFocusableView to cycle back
  //           to the first view within this root when the traversal reaches
  //           the end. If this is true, then if you pass a valid starting
  //           view to FindNextFocusableView you will always get a valid view
  //           out, even if it's the same view.
  // - |accessibility_mode| should be true if full keyboard accessibility is
  //   needed and you want to check
  //   GetViewAccessibility().IsAccessibilityFocusable(), rather than
  //   IsFocusable().
  FocusSearch(View* root, bool cycle, bool accessibility_mode);

  FocusSearch(const FocusSearch&) = delete;
  FocusSearch& operator=(const FocusSearch&) = delete;
  virtual ~FocusSearch() = default;

  // Finds the next view that should be focused and returns it. If a
  // FocusTraversable is found while searching for the focusable view,
  // returns NULL and sets |focus_traversable| to the FocusTraversable
  // and |focus_traversable_view| to the view associated with the
  // FocusTraversable.
  //
  // Return NULL if the end of the focus loop is reached, unless this object
  // was initialized with |cycle|=true, in which case it goes back to the
  // beginning when it reaches the end of the traversal.
  // - |starting_view| is the view that should be used as the starting point
  //   when looking for the previous/next view. It may be NULL (in which case
  //   the first/last view should be used depending if normal/reverse).
  // - |search_direction| whether we should find the next (kForwards) or
  //   previous (kReverse) view.
  // - |traversal_direction| specifies whether we are traversing down (meaning
  //   we should look into child views) or traversing up (don't look at
  //   child views).
  // - |check_starting_view| indicated if starting_view may obtain the next
  //   focus.
  // - |can_go_into_anchored_dialog| controls if focus is allowed to jump
  //   into a dialog anchored at one of the views being traversed.
  // - |focus_traversable| is set to the focus traversable that should be
  //   traversed if one is found (in which case the call returns NULL).
  // - |focus_traversable_view| is set to the view associated with the
  //   FocusTraversable set in the previous parameter (it is used as the
  //   starting view when looking for the next focusable view).
  virtual View* FindNextFocusableView(
      View* starting_view,
      SearchDirection search_direction,
      TraversalDirection traversal_direction,
      StartingViewPolicy check_starting_view,
      AnchoredDialogPolicy can_go_into_anchored_dialog,
      FocusTraversable** focus_traversable,
      View** focus_traversable_view);

 protected:
  // Get the parent, but stay within the root. Returns NULL if asked for
  // the parent of |root_|. Subclasses can override this if they need custom
  // focus search behavior.
  virtual View* GetParent(View* v);

  // Returns true if |v| is contained within the hierarchy rooted at |root|.
  // Subclasses can override this if they need custom focus search behavior.
  virtual bool Contains(View* root, const View* v);

  View* root() const { return root_; }

 private:
  // Convenience method that returns true if a view is focusable and does not
  // belong to the specified group.
  bool IsViewFocusableCandidate(View* v, int skip_group_id);

  // Convenience method; returns true if a view is not NULL and is focusable
  // (checking IsAccessibilityFocusable() if |accessibility_mode_| is true or
  // the associated FocusManager has keyboard accessibility enabled).
  bool IsFocusable(View* v);

  // Returns the view selected for the group of the selected view. If the view
  // does not belong to a group or if no view is selected in the group, the
  // specified view is returned.
  View* FindSelectedViewForGroup(View* view);

  // Returns the next focusable view or view containing a FocusTraversable
  // (NULL if none was found), starting at the starting_view.
  // |check_starting_view|, |can_go_up| and |can_go_down| controls the
  // traversal of the views hierarchy. |skip_group_id| specifies a group_id,
  // -1 means no group. All views from a group are traversed in one pass.
  View* FindNextFocusableViewImpl(
      View* starting_view,
      StartingViewPolicy check_starting_view,
      bool can_go_up,
      bool can_go_down,
      AnchoredDialogPolicy can_go_into_anchored_dialog,
      int skip_group_id,
      base::flat_set<View*>* seen_views,
      FocusTraversable** focus_traversable,
      View** focus_traversable_view);

  // Same as FindNextFocusableViewImpl but returns the previous focusable view.
  View* FindPreviousFocusableViewImpl(
      View* starting_view,
      StartingViewPolicy check_starting_view,
      bool can_go_up,
      bool can_go_down,
      AnchoredDialogPolicy can_go_into_anchored_dialog,
      int skip_group_id,
      base::flat_set<View*>* seen_views,
      FocusTraversable** focus_traversable,
      View** focus_traversable_view);

  raw_ptr<View> root_;
  bool cycle_;
  bool accessibility_mode_;
};

}  // namespace views

#endif  // UI_VIEWS_FOCUS_FOCUS_SEARCH_H_
