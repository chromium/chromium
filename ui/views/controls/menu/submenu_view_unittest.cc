// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/submenu_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/test/views_test_base.h"

namespace views {

using SubmenuViewTest = ViewsTestBase;

TEST_F(SubmenuViewTest, GetLastItem) {
  auto parent_owning = std::make_unique<MenuItemView>();
  MenuItemView* parent = parent_owning.get();
  MenuRunner menu_runner(std::move(parent_owning), 0);

  SubmenuView* submenu = parent->CreateSubmenu();
  EXPECT_EQ(nullptr, submenu->GetLastItem());

  submenu->AddChildView(std::make_unique<View>());
  EXPECT_EQ(nullptr, submenu->GetLastItem());

  MenuItemView* first = submenu->AddChildView(std::make_unique<MenuItemView>());
  EXPECT_EQ(first, submenu->GetLastItem());

  submenu->AddChildView(std::make_unique<View>());
  EXPECT_EQ(first, submenu->GetLastItem());

  MenuItemView* second =
      submenu->AddChildView(std::make_unique<MenuItemView>());
  EXPECT_EQ(second, submenu->GetLastItem());
}

TEST_F(SubmenuViewTest, AccessibleProperties) {
  auto parent_owning = std::make_unique<MenuItemView>();
  MenuItemView* parent = parent_owning.get();
  MenuRunner menu_runner(std::move(parent_owning), 0);
  SubmenuView* submenu = parent->CreateSubmenu();
  EXPECT_EQ(nullptr, submenu->GetLastItem());

  ui::AXNodeData data;
  submenu->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kMenu, data.role);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kVertical));
}

}  // namespace views
