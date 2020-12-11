// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_TRACKER_H_
#define UI_VIEWS_VIEW_TRACKER_H_

#include "base/scoped_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {
// ViewTracker tracks a single View. When the View is deleted it's removed.
class VIEWS_EXPORT ViewTracker : public ViewObserver {
 public:
  explicit ViewTracker(View* view = nullptr);
  ViewTracker(const ViewTracker&) = delete;
  ViewTracker& operator=(const ViewTracker&) = delete;
  ~ViewTracker() override;

  void SetView(View* view);
  View* view() { return view_; }

  // ViewObserver:
  void OnViewIsDeleting(View* observed_view) override;

 private:
  View* view_ = nullptr;

  ScopedObserver<View, ViewObserver> observer_{this};
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_TRACKER_H_
