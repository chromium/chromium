// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_targeter_delegate.h"

#include <limits.h>

#include "base/containers/adapters.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view.h"

namespace {

// The minimum percentage of a view's area that needs to be covered by a rect
// representing a touch region in order for that view to be considered by the
// rect-based targeting algorithm.
static const float kRectTargetOverlap = 0.6f;

}  // namespace

namespace views {

// TODO(tdanderson): Move the contents of rect_based_targeting_utils.(h|cc)
//                   into here.

bool ViewTargeterDelegate::DoesIntersectRect(const View* target,
                                             const gfx::Rect& rect) const {
  return target->GetLocalBounds().Intersects(rect);
}

View* ViewTargeterDelegate::TargetForRect(View* root, const gfx::Rect& rect) {
  // |rect_view| represents the current best candidate to return
  // if rect-based targeting (i.e., fuzzing) is used.
  // |rect_view_distance| is used to keep track of the distance
  // between the center point of |rect_view| and the center
  // point of |rect|.
  View* rect_view = nullptr;
  int rect_view_distance = INT_MAX;

  // |point_view| represents the view that would have been returned
  // from this function call if point-based targeting were used.
  View* point_view = nullptr;

  View::Views children = root->GetChildrenInZOrder();
  DCHECK_EQ(root->children().size(), children.size());
  for (views::View* child : base::Reversed(children)) {
    if (!child->GetCanProcessEventsWithinSubtree() || !child->GetEnabled())
      continue;

    // Ignore any children which are invisible or do not intersect |rect|.
    if (!child->GetVisible())
      continue;
    gfx::RectF rect_in_child_coords_f(rect);
    View::ConvertRectToTarget(root, child, &rect_in_child_coords_f);
    gfx::Rect rect_in_child_coords =
        gfx::ToEnclosingRect(rect_in_child_coords_f);
    if (!child->HitTestRect(rect_in_child_coords))
      continue;

    View* cur_view = child->GetEventHandlerForRect(rect_in_child_coords);

    if (views::UsePointBasedTargeting(rect))
      return cur_view;

    gfx::RectF cur_view_bounds_f(cur_view->GetLocalBounds());
    View::ConvertRectToTarget(cur_view, root, &cur_view_bounds_f);
    gfx::Rect cur_view_bounds = gfx::ToEnclosingRect(cur_view_bounds_f);
    if (views::PercentCoveredBy(cur_view_bounds, rect) >= kRectTargetOverlap) {
      // |cur_view| is a suitable candidate for rect-based targeting.
      // Check to see if it is the closest suitable candidate so far.
      gfx::Point touch_center(rect.CenterPoint());
      int cur_dist = views::DistanceSquaredFromCenterToPoint(touch_center,
                                                             cur_view_bounds);
      if (!rect_view || cur_dist < rect_view_distance) {
        rect_view = cur_view;
        rect_view_distance = cur_dist;
      }
    } else if (!rect_view && !point_view) {
      // Rect-based targeting has not yielded any candidates so far. Check
      // if point-based targeting would have selected |cur_view|.
      gfx::Point point_in_child_coords(rect_in_child_coords.CenterPoint());
      if (child->HitTestPoint(point_in_child_coords))
        point_view = child->GetEventHandlerForPoint(point_in_child_coords);
    }
  }

  if (views::UsePointBasedTargeting(rect) || (!rect_view && !point_view))
    return root;

  return rect_view ? rect_view : point_view;
}

}  // namespace views
