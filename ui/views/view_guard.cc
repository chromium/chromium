// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_guard.h"

#include "base/notreached.h"
#include "ui/views/view.h"

namespace views {

ViewGuard::ViewGuard(View& view) : view_(view) {
  observation_.Observe(&*view_);
}
ViewGuard::ViewGuard(const ViewGuard& other) : ViewGuard(*other.view_) {}
ViewGuard::~ViewGuard() = default;

void ViewGuard::OnViewIsDeleting(View* view) {
  NOTREACHED() << "View " << view->GetClassName()
               << " is being deleted during the scope of a guard.";
}

}  // namespace views
