// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_guard.h"

#include <optional>

#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "ui/views/view.h"

namespace views {

ViewGuard::ViewGuard(View& view,
                     std::optional<base::NotFatalUntil> not_fatal_until)
    : view_(&view), not_fatal_until_(not_fatal_until) {
  observation_.Observe(view_);
}
ViewGuard::ViewGuard(const ViewGuard& other)
    : view_(other.view_), not_fatal_until_(other.not_fatal_until_) {
  if (view_) {
    observation_.Observe(view_);
  }
}
ViewGuard::~ViewGuard() = default;

void ViewGuard::OnViewIsDeleting(View* view) {
  if (not_fatal_until_) {
    NOTREACHED(not_fatal_until_.value())
        << "View " << view->GetClassName()
        << " is being deleted during the scope of a guard.";
    view_ = nullptr;
    observation_.Reset();
  } else {
    NOTREACHED() << "View " << view->GetClassName()
                 << " is being deleted during the scope of a guard.";
  }
}

}  // namespace views
