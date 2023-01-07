// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_manager.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_layout_manager.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view.h"

namespace views {

TEST(LayoutManagerTest, SetVisibleInvalidatesLayout) {
  auto host = std::make_unique<View>();
  View* child = host->AddChildView(std::make_unique<View>());
  test::TestLayoutManager* layout_manager =
      host->SetLayoutManager(std::make_unique<test::TestLayoutManager>());
  child->SetVisible(false);
  EXPECT_EQ(1, layout_manager->invalidate_count());
  child->SetVisible(true);
  EXPECT_EQ(2, layout_manager->invalidate_count());
}

TEST(LayoutManagerTest, SetVisibleUnchangedDoesNotInvalidateLayout) {
  auto host = std::make_unique<View>();
  View* child = host->AddChildView(std::make_unique<View>());
  test::TestLayoutManager* layout_manager =
      host->SetLayoutManager(std::make_unique<test::TestLayoutManager>());
  child->SetVisible(true);
  EXPECT_EQ(0, layout_manager->invalidate_count());
  child->SetVisible(false);
  EXPECT_EQ(1, layout_manager->invalidate_count());
  child->SetVisible(false);
  EXPECT_EQ(1, layout_manager->invalidate_count());
}

}  // namespace views
