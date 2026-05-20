// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/submenu_view.h"

#include <memory>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/views/accessibility/tree/widget_ax_manager_test_api.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

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
  EXPECT_TRUE(data.HasState(ax::mojom::State::kIgnored));
}

TEST_F(SubmenuViewTest, AccessibilityIgnoredStateFollowsMenuVisibility) {
  auto owner = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  owner->Show();

  auto menu_delegate = std::make_unique<MenuDelegate>();
  auto root_owning = std::make_unique<MenuItemView>(menu_delegate.get());
  MenuItemView* root = root_owning.get();
  root->AppendMenuItem(1, u"Item");
  MenuRunner menu_runner(std::move(root_owning), 0);

  SubmenuView* submenu = root->GetSubmenu();
  EXPECT_TRUE(submenu->GetViewAccessibility().GetIsIgnored());

  menu_runner.RunMenuAt(owner.get(), nullptr, gfx::Rect(),
                        MenuAnchorPosition::kTopLeft,
                        ui::mojom::MenuSourceType::kNone);
  ASSERT_TRUE(menu_runner.IsRunning());
  EXPECT_FALSE(submenu->GetViewAccessibility().GetIsIgnored());

  menu_runner.Cancel();
  EXPECT_TRUE(submenu->GetViewAccessibility().GetIsIgnored());

  owner->CloseNow();
}

#if BUILDFLAG(HAS_NATIVE_ACCESSIBILITY)
TEST_F(SubmenuViewTest, NestedSubmenuHostsChildAXTreeOnOwningMenuItem) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAccessibilityTreeForViews);
  ui::ScopedAXModeSetter enable_accessibility(ui::AXMode::kNativeAPIs);

  auto owner = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  owner->Show();

  auto menu_delegate = std::make_unique<MenuDelegate>();
  auto root_owning = std::make_unique<MenuItemView>(menu_delegate.get());
  MenuItemView* root = root_owning.get();
  root->AppendMenuItem(1, u"Root item");
  MenuItemView* submenu_item = root->AppendSubMenu(2, u"Submenu item");
  submenu_item->AppendMenuItem(3, u"Child item");
  MenuRunner menu_runner(std::move(root_owning), 0);

  EXPECT_EQ(ui::AXTreeIDUnknown(),
            submenu_item->GetViewAccessibility().GetChildTreeID());

  menu_runner.RunMenuAt(owner.get(), nullptr, gfx::Rect(),
                        MenuAnchorPosition::kTopLeft,
                        ui::mojom::MenuSourceType::kNone);
  ASSERT_TRUE(menu_runner.IsRunning());

  MenuController::GetActiveInstance()->SelectItemAndOpenSubmenu(submenu_item);
  SubmenuView* child_submenu = submenu_item->GetSubmenu();
  ASSERT_NE(nullptr, child_submenu->GetWidget());
  ASSERT_NE(nullptr, child_submenu->GetWidget()->ax_manager());
  ASSERT_NE(nullptr, submenu_item->GetWidget());
  ASSERT_NE(nullptr, submenu_item->GetWidget()->ax_manager());

  WidgetAXManager* child_manager = child_submenu->GetWidget()->ax_manager();
  WidgetAXManager* parent_manager = submenu_item->GetWidget()->ax_manager();
  ui::AXTreeID child_tree_id;
  {
    WidgetAXManagerTestApi child_api(child_manager);
    WidgetAXManagerTestApi parent_api(parent_manager);
    child_tree_id = child_api.ax_tree_id();
    EXPECT_EQ(child_tree_id,
              submenu_item->GetViewAccessibility().GetChildTreeID());
    EXPECT_EQ(parent_api.ax_tree_id(), child_api.parent_ax_tree_id());
  }

  menu_runner.Cancel();
  EXPECT_TRUE(child_submenu->GetViewAccessibility().GetIsIgnored());
  EXPECT_EQ(child_tree_id,
            submenu_item->GetViewAccessibility().GetChildTreeID());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return submenu_item->GetViewAccessibility().GetChildTreeID() ==
           ui::AXTreeIDUnknown();
  }));

  owner->CloseNow();
}
#endif  // BUILDFLAG(HAS_NATIVE_ACCESSIBILITY)

}  // namespace views
