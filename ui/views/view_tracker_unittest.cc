// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_tracker.h"

#include <memory>

#include "base/functional/bind.h"
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
  base::MockCallback<base::OnceClosure> is_deleting_callback;
  EXPECT_CALL(is_deleting_callback, Run);
  ViewTracker tracker;
  {
    std::unique_ptr<View> tracked_view = std::make_unique<View>();
    tracker.SetView(tracked_view.get());
    EXPECT_EQ(tracked_view.get(), tracker.view());
    tracker.SetIsDeletingCallback(is_deleting_callback.Get());
  }
}

TEST_F(ViewTrackerTest, TestTrackEntireViewHierarchy) {
  auto hierarchy_deleting_callback = [](View* view) {
    // Expected to be called before children are destroyed.
    EXPECT_EQ(view->children().size(), 1u);
  };
  ViewTracker tracker;
  {
    tracker.SetTrackEntireViewHierarchy(true);
    std::unique_ptr<View> tracked_view = std::make_unique<View>();
    tracked_view->AddChildView(std::make_unique<View>());
    tracker.SetView(tracked_view.get());
    tracker.SetIsDeletingCallback(base::BindOnce(
        hierarchy_deleting_callback, base::Unretained(tracked_view.get())));
  }
  auto view_deleting_callback = [](View* view) {
    // Expected to be called after children are destroyed.
    EXPECT_EQ(view->children().size(), 0u);
  };
  {
    tracker.SetTrackEntireViewHierarchy(false);
    std::unique_ptr<View> tracked_view = std::make_unique<View>();
    tracked_view->AddChildView(std::make_unique<View>());
    tracker.SetView(tracked_view.get());
    tracker.SetIsDeletingCallback(base::BindOnce(
        view_deleting_callback, base::Unretained(tracked_view.get())));
  }
}

}  // namespace views
