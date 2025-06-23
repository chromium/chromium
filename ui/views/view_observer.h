// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_OBSERVER_H_
#define UI_VIEWS_VIEW_OBSERVER_H_

#include <stdint.h>

#include "ui/views/views_export.h"

namespace views {

class View;
struct ViewHierarchyChangedDetails;

// ViewObserver is used to observe changes to a View. The first argument to all
// ViewObserver functions is the View the observer was added to.
class VIEWS_EXPORT ViewObserver {
 public:
  // Called when |child| is added as a child to |observed_view|.
  virtual void OnChildViewAdded(View* observed_view, View* child) {}

  // Called when |child| is removed as a child of |observed_view|.
  virtual void OnChildViewRemoved(View* observed_view, View* child) {}

  // Called when |observed_view|, an ancestor, or its Widget has its visibility
  // changed. |starting_view| is who |View::SetVisible()| was called on (or null
  // if the Widget visibility changed).
  virtual void OnViewVisibilityChanged(View* observed_view,
                                       View* starting_view) {}

  // Called from View::PreferredSizeChanged().
  virtual void OnViewPreferredSizeChanged(View* observed_view) {}

  // Called when the bounds of |observed_view| change.
  virtual void OnViewBoundsChanged(View* observed_view) {}

  // Called when the view layer's bounds are set, whether or not the bounds have
  // changed.
  virtual void OnViewLayerBoundsSet(View* observed_view) {}

  // Called when the `observed_view`'s layer transform changes.
  // TODO(crbug.com/40763515): This is temporarily added to support a migration.
  // Do not use for new call sites, we should instead figure out how to
  // migrate this method (and possibly others) into callbacks.
  virtual void OnViewLayerTransformed(View* observed_view) {}

  // Called when `observed_view`'s layer clip rect changes.
  virtual void OnViewLayerClipRectChanged(View* observed_view) {}

  // Called when View::ViewHierarchyChanged() is called.
  virtual void OnViewHierarchyChanged(
      View* observed_view,
      const ViewHierarchyChangedDetails& details) {}

  // Called when View::AddedToWidget() is called.
  virtual void OnViewAddedToWidget(View* observed_view) {}

  // Called when View::RemovedFromWidget() is called.
  virtual void OnViewRemovedFromWidget(View* observed_view) {}

  // Called when a child is reordered among its siblings, specifically
  // View::ReorderChildView() is called on |observed_view|.
  virtual void OnChildViewReordered(View* observed_view, View* child) {}

  // Called when the active UI theme or NativeTheme has changed for
  // |observed_view|.
  virtual void OnViewThemeChanged(View* observed_view) {}

  // Called from ~View.
  virtual void OnViewIsDeleting(View* observed_view) {}

  // Called from the very beginning of ~View (before child views are deleted) to
  // indicate that destruction is starting at the hierarchy owned by
  // `observed_view`.
  virtual void OnViewHierarchyWillBeDeleted(View* observed_view) {}

  // Called immediately after |observed_view| has gained focus.
  virtual void OnViewFocused(View* observed_view) {}

  // Called immediately after |observed_view| has lost focus.
  virtual void OnViewBlurred(View* observed_view) {}

  // Called immediately after the property associated with the specified
  // |key| has been changed for the |observed_view|. The |old_value| must be
  // cast to the appropriate type before use, see |ui::ClassPropertyCaster|.
  virtual void OnViewPropertyChanged(View* observed_view,
                                     const void* key,
                                     int64_t old_value) {}

  // Called when the observed view's layout is invalidated.
  // This is useful to invalidate other views in response to the observed
  // view's layout invalidation, for example, when the view to be invalidated
  // has style dependency on the observed view.
  virtual void OnViewLayoutInvalidated(View* observed_view) {}

 protected:
  virtual ~ViewObserver() = default;
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_OBSERVER_H_
