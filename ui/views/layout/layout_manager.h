// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_LAYOUT_MANAGER_H_
#define UI_VIEWS_LAYOUT_LAYOUT_MANAGER_H_

#include <vector>

#include "ui/views/layout/layout_types.h"
#include "ui/views/views_export.h"

namespace gfx {
class Size;
}

namespace views {

class View;

// LayoutManager is used by View to accomplish the following:
//
// . Provides preferred sizing information, see GetPreferredSize() and
//   GetPreferredHeightForWidth().
// . To position and size (aka layout) the children of the associated View. See
//   Layout() for details.
//
// How a LayoutManager operates is specific to the LayoutManager. Non-trivial
// LayoutManagers calculate preferred size and layout information using the
// minimum and preferred size of the children of the View. That is, they
// make use of View::GetMinimumSize(), View::CalculatePreferredSize() and/or
// View::GetHeightForWidth().
class VIEWS_EXPORT LayoutManager {
 public:
  virtual ~LayoutManager();

  // Notification that this LayoutManager has been installed on |host|.
  virtual void Installed(View* host);

  // For layout managers that can cache layout data, it's useful to let the
  // layout manager know that its current layout might not be valid.
  // TODO(dfried): consider if we should include some default behavior (like a
  // rolling layout counter).
  virtual void InvalidateLayout();

  // Called by View::Layout() to position and size the children of |host|.
  // Generally this queries |host| for its size and positions and sizes the
  // children in a LayoutManager specific way.
  virtual void Layout(View* host) = 0;

  // Returns the preferred size, which is typically the size needed to give each
  // child of |host| its preferred size. Generally this is calculated using the
  // View::CalculatePreferredSize() on each of the children of |host|.
  virtual gfx::Size GetPreferredSize(const View* host) const = 0;

  // Returns the minimum size, which defaults to the preferred size. Layout
  // managers with the ability to collapse or hide child views may override this
  // behavior.
  virtual gfx::Size GetMinimumSize(const View* host) const;

  // Return the preferred height for a particular width. Generally this is
  // calculated using View::GetHeightForWidth() or
  // View::CalculatePreferredSize() on each of the children of |host|. Override
  // this function if the preferred height varies based on the size. For
  // example, a multi-line labels preferred height may change with the width.
  // The default implementation returns GetPreferredSize().height().
  virtual int GetPreferredHeightForWidth(const View* host, int width) const;

  // Returns the maximum space available in the layout for the specified child
  // view. Default is unbounded.
  virtual SizeBounds GetAvailableSize(const View* host, const View* view) const;

  // Called when a View is added as a child of the View the LayoutManager has
  // been installed on.
  virtual void ViewAdded(View* host, View* view);

  // Called when a View is removed as a child of the View the LayoutManager has
  // been installed on. This function allows the LayoutManager to cleanup any
  // state it has kept specific to a View.
  virtual void ViewRemoved(View* host, View* view);

  // Called when View::SetVisible() is called by external code. Classes derived
  // from LayoutManager can call SetViewVisibility() below to avoid triggering
  // this event. Note that |old_visibility| and |new_visibility| can be the
  // same, because the old visibility may have been set by the layout and not
  // external code.
  virtual void ViewVisibilitySet(View* host,
                                 View* view,
                                 bool old_visibility,
                                 bool new_visibility);

 protected:
  // Sets the visibility of a view without triggering ViewVisibilitySet().
  // During Layout(), use this method instead of View::SetVisibility().
  void SetViewVisibility(View* view, bool visible);

  // Gets the child views of the specified view in paint order (reverse
  // Z-order). Defaults to returning host->children(). Called by
  // View::GetChildrenInZOrder().
  virtual std::vector<View*> GetChildViewsInPaintOrder(const View* host) const;

 private:
  friend class views::View;
  View* view_setting_visibility_on_ = nullptr;
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_LAYOUT_MANAGER_H_
