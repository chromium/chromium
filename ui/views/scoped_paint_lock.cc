// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/scoped_paint_lock.h"

#include "ui/views/view.h"

namespace views {

ScopedPaintLock::ScopedPaintLock(View* view) {
  view_observation_.Observe(view);
  view->AddPaintLock();
}

ScopedPaintLock::~ScopedPaintLock() {
  if (view_observation_.IsObserving()) {
    View* view = view_observation_.GetSource();
    view_observation_.Reset();
    view->RemovePaintLock();
  }
}

void ScopedPaintLock::OnViewIsDeleting(View* observed_view) {
  view_observation_.Reset();
}

}  // namespace views
