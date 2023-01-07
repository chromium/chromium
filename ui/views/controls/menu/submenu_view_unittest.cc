// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/submenu_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/test/views_test_base.h"

namespace views {

using SubmenuViewTest = ViewsTestBase;

TEST_F(SubmenuViewTest, GetLastItem) {
  MenuItemView* parent = new MenuItemView();
  MenuRunner menu_runner(parent, 0);

  SubmenuView* submenu = parent->CreateSubmenu();
  EXPECT_EQ(nullptr, submenu->GetLastItem());

  submenu->AddChildView(new View());
  EXPECT_EQ(nullptr, submenu->GetLastItem());

  MenuItemView* first = new MenuItemView();
  submenu->AddChildView(first);
  EXPECT_EQ(first, submenu->GetLastItem());

  submenu->AddChildView(new View());
  EXPECT_EQ(first, submenu->GetLastItem());

  MenuItemView* second = new MenuItemView();
  submenu->AddChildView(second);
  EXPECT_EQ(second, submenu->GetLastItem());
}

}  // namespace views
