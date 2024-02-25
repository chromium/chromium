// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_TRACKER_H_
#define UI_VIEWS_VIEW_TRACKER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// ViewTracker tracks a single View. When the View is deleted it's removed.
class VIEWS_EXPORT ViewTracker : public ViewObserver {
 public:
  explicit ViewTracker(View* view = nullptr);
  ViewTracker(const ViewTracker&) = delete;
  ViewTracker& operator=(const ViewTracker&) = delete;
  ~ViewTracker() override;

  void SetView(View* view);
  View* view() { return view_; }
  const View* view() const { return view_; }
  // If `track_entire_view_hierarchy_` is true this will be run before
  // destruction of the view hierarchy. If `track_entire_view_hierarchy_` is
  // false the callback will be run right before the view itself will be deleted
  // (child views will already be destroyed at that point).
  void SetIsDeletingCallback(base::OnceClosure is_deleting_callback);
  // If set to true, the view tracker will remove the view it is tracking before
  // destruction of the view hierarchy, including child views, starts.
  void SetTrackEntireViewHierarchy(bool track_entire_view_hierarchy) {
    track_entire_view_hierarchy_ = track_entire_view_hierarchy;
  }

  operator bool() const { return !!view_; }

  // ViewObserver:
  void OnViewIsDeleting(View* observed_view) override;
  void OnViewHierarchyWillBeDeleted(View* observed_view) override;

 private:
  void HandleViewDestruction();

  raw_ptr<View> view_ = nullptr;

  base::OnceClosure is_deleting_callback_;

  bool track_entire_view_hierarchy_ = false;

  base::ScopedObservation<View, ViewObserver> observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_TRACKER_H_
