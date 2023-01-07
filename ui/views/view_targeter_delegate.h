// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_TARGETER_DELEGATE_H_
#define UI_VIEWS_VIEW_TARGETER_DELEGATE_H_

#include "ui/views/views_export.h"

namespace gfx {
class Rect;
}

namespace views {
class View;

// Defines the default behaviour for hit-testing and event-targeting against a
// View using a rectangular region representing an event's location (i.e., the
// bounding box of a gesture or a 1x1 rect in the case of a mouse event). Views
// wishing to define custom hit-testing or event-targeting behaviour do so by
// extending ViewTargeterDelegate and then installing a ViewTargeter on
// themselves.
class VIEWS_EXPORT ViewTargeterDelegate {
 public:
  ViewTargeterDelegate() = default;

  ViewTargeterDelegate(const ViewTargeterDelegate&) = delete;
  ViewTargeterDelegate& operator=(const ViewTargeterDelegate&) = delete;

  virtual ~ViewTargeterDelegate() = default;

  // Returns true if |target| should be considered as a candidate target for
  // an event having |rect| as its location, where |rect| is in the local
  // coordinate space of |target|. Overrides of this method by a View subclass
  // should enforce DCHECK_EQ(this, target).
  // TODO(tdanderson): Consider changing the name of this method to better
  //                   reflect its purpose.
  virtual bool DoesIntersectRect(const View* target,
                                 const gfx::Rect& rect) const;

  // If point-based targeting should be used, return the deepest visible
  // descendant of |root| that contains the center point of |rect|.
  // If rect-based targeting (i.e., fuzzing) should be used, return the
  // closest visible descendant of |root| having at least kRectTargetOverlap of
  // its area covered by |rect|. If no such descendant exists, return the
  // deepest visible descendant of |root| that contains the center point of
  // |rect|. See http://goo.gl/3Jp2BD for more information about rect-based
  // targeting.
  virtual View* TargetForRect(View* root, const gfx::Rect& rect);
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_TARGETER_DELEGATE_H_
