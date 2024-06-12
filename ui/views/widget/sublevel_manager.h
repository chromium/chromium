// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_SUBLEVEL_MANAGER_H_
#define UI_VIEWS_WIDGET_SUBLEVEL_MANAGER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

// The SublevelManager ensures a widget is shown at the correct sublevel.
// It tracks the sublevel of the owner widget and the stacking state of
// the owner's children widgets.
class VIEWS_EXPORT SublevelManager : public WidgetObserver {
 public:
  SublevelManager(Widget* owner, int sublevel);

  ~SublevelManager() override;

  // Tracks a child widget.
  void TrackChildWidget(Widget* child);

  // Untracks a child widget.
  // This is intended for internal use and to work around platform-specific
  // compatibility issues.
  // You should not use this.
  void UntrackChildWidget(Widget* child);

  // Sets the sublevel of `owner_` and triggers `EnsureOwnerSublevel()`.
  void SetSublevel(int sublevel);

  // Gets the sublevel of `owner_`.
  int GetSublevel() const;

  // Repositions `owner_` among its siblings of the same z-order level
  // to ensure that its sublevel is respected.
  void EnsureOwnerSublevel();

  // Repositions `owner_` and its descendants to ensure that their sublevels
  // are respected.
  void EnsureOwnerTreeSublevel();

 private:
  // WidgetObserver:
  void OnWidgetDestroying(Widget* owner) override;

  // Repositions `child_` among its siblings of the same z-order level
  // to ensure that its sublevel is respected.
  void OrderChildWidget(Widget* child);

  // Check if a child widget is being tracked.
  bool IsTrackingChildWidget(Widget* child);

  // Returns the position in `children_` before which `child` should be inserted
  // to maintain the sublevel ordering. This methods assumes that `child` is not
  // in `children_`.
  using ChildIterator =
      std::vector<raw_ptr<Widget, VectorExperimental>>::const_iterator;
  ChildIterator FindInsertPosition(Widget* child) const;

  // The owner widget.
  raw_ptr<Widget> owner_;

  // The sublevel of `owner_`.
  int sublevel_;

  // The observation of `owner_`.
  base::ScopedObservation<Widget, WidgetObserver> owner_observation_{this};

  // The tracked children widgets in the actual, back-to-front stacking order.
  // After ensuring sublevel, children should be ordered by their sublevel
  // within each level subsequence, but subsequences may interleave with each
  // other. For example, "[(1,0), (2,0), (2,1), (1,1), (1,2)]" is a possible
  // sequence of (level, sublevel).
  std::vector<raw_ptr<Widget, VectorExperimental>> children_;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_SUBLEVEL_MANAGER_H_
