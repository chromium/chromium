// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_tracker.h"

#include <memory>

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

}  // namespace views
