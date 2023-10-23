// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_tracker.h"
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

void ViewTracker::OnViewIsDeleting(View* observed_view) {
  SetView(nullptr);
}

}  // namespace views
