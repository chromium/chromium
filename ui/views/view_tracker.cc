// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_tracker.h"

#include <utility>

#include "base/functional/callback.h"
#include "ui/views/view.h"

namespace views {

ViewTracker::ViewTracker(View* view) {
  SetView(view);
}

ViewTracker::~ViewTracker() = default;

void ViewTracker::SetView(View* view) {
  if (view == view_)
    return;

  observation_.Reset();
  view_ = view;
  if (view_)
    observation_.Observe(view_.get());
}

void ViewTracker::SetIsDeletingCallback(
    base::OnceClosure is_deleting_callback) {
  is_deleting_callback_ = std::move(is_deleting_callback);
}

void ViewTracker::OnViewIsDeleting(View* observed_view) {
  if (track_entire_view_hierarchy_) {
    return;
  }
  HandleViewDestruction();
}

void ViewTracker::OnViewHierarchyWillBeDeleted(View* observed_view) {
  if (!track_entire_view_hierarchy_) {
    return;
  }
  HandleViewDestruction();
}

void ViewTracker::HandleViewDestruction() {
  // View is already in destructor. Set to nullptr first before running any
  // callbacks.
  SetView(nullptr);
  if (is_deleting_callback_.is_null()) {
    return;
  }
  std::move(is_deleting_callback_).Run();
}

}  // namespace views
