// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_model_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace views {

// Makes sure SetViewBoundsToIdealBounds updates the view appropriately.
TEST(ViewModelUtils, SetViewBoundsToIdealBounds) {
  View v1;
  ViewModel model;
  model.Add(&v1, 0);
  gfx::Rect v1_bounds(1, 2, 3, 4);
  model.set_ideal_bounds(0, v1_bounds);
  ViewModelUtils::SetViewBoundsToIdealBounds(model);
  EXPECT_EQ(v1_bounds, v1.bounds());
}

// Assertions for DetermineMoveIndex.
TEST(ViewModelUtils, DetermineMoveIndex) {
  View v1, v2, v3;
  ViewModel model;
  model.Add(&v1, 0);
  model.Add(&v2, 1);
  model.Add(&v3, 2);
  model.set_ideal_bounds(0, gfx::Rect(0, 0, 10, 10));
  model.set_ideal_bounds(1, gfx::Rect(10, 0, 1000, 10));
  model.set_ideal_bounds(2, gfx::Rect(1010, 0, 2, 10));

  EXPECT_EQ(0, ViewModelUtils::DetermineMoveIndex(model, &v1, true, -10, 0));
  EXPECT_EQ(0, ViewModelUtils::DetermineMoveIndex(model, &v1, true, 4, 0));
  EXPECT_EQ(1, ViewModelUtils::DetermineMoveIndex(model, &v1, true, 506, 0));
  EXPECT_EQ(2, ViewModelUtils::DetermineMoveIndex(model, &v1, true, 1010, 0));
  EXPECT_EQ(2, ViewModelUtils::DetermineMoveIndex(model, &v1, true, 2000, 0));

  EXPECT_EQ(0, ViewModelUtils::DetermineMoveIndex(model, &v2, true, -10, 0));
  EXPECT_EQ(0, ViewModelUtils::DetermineMoveIndex(model, &v2, true, 4, 0));
  EXPECT_EQ(2, ViewModelUtils::DetermineMoveIndex(model, &v2, true, 12, 0));

  // Try the same when vertical.
  model.set_ideal_bounds(0, gfx::Rect(0, 0, 10, 10));
  model.set_ideal_bounds(1, gfx::Rect(0, 10, 10, 1000));
  model.set_ideal_bounds(2, gfx::Rect(0, 1010, 10, 2));

  EXPECT_EQ(0, ViewModelUtils::DetermineMoveIndex(model, &v1, false, 0, -10));
  EXPECT_EQ(0, ViewModelUtils::DetermineMoveIndex(model, &v1, false, 0, 4));
  EXPECT_EQ(1, ViewModelUtils::DetermineMoveIndex(model, &v1, false, 0, 506));
  EXPECT_EQ(2, ViewModelUtils::DetermineMoveIndex(model, &v1, false, 0, 1010));
  EXPECT_EQ(2, ViewModelUtils::DetermineMoveIndex(model, &v1, false, 0, 2000));

  EXPECT_EQ(0, ViewModelUtils::DetermineMoveIndex(model, &v2, false, 0, -10));
  EXPECT_EQ(0, ViewModelUtils::DetermineMoveIndex(model, &v2, false, 0, 4));
  EXPECT_EQ(2, ViewModelUtils::DetermineMoveIndex(model, &v2, false, 0, 12));
}

}  // namespace views
