// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_tracker.h"

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
    observation_.Observe(view_);
}

void ViewTracker::OnViewIsDeleting(View* observed_view) {
  SetView(nullptr);
}

}  // namespace views
