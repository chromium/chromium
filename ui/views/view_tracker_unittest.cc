// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_tracker.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace views {

using ViewTrackerTest = ViewsTestBase;

TEST_F(ViewTrackerTest, RemovedOnDelete) {
  ViewTracker tracker;
  {
    View view;
    tracker.SetView(&view);
    EXPECT_EQ(&view, tracker.view());
  }
  EXPECT_EQ(nullptr, tracker.view());
}

TEST_F(ViewTrackerTest, ObservedAtConstruction) {
  std::unique_ptr<ViewTracker> tracker;
  {
    View view;
    tracker = std::make_unique<ViewTracker>(&view);
    EXPECT_EQ(&view, tracker->view());
  }
  EXPECT_EQ(nullptr, tracker->view());
}

TEST_F(ViewTrackerTest, RunCallbackOnViewDeletion) {
  base::MockCallback<base::OnceClosure> on_view_is_deleting_callback;
  EXPECT_CALL(on_view_is_deleting_callback, Run);
  ViewTracker tracker;
  {
    View view;
    tracker.SetView(&view);
    EXPECT_EQ(&view, tracker.view());
    tracker.SetOnViewIsDeletingCallback(on_view_is_deleting_callback.Get());
  }
}

}  // namespace views
