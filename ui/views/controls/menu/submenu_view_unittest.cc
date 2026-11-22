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
#include "ui/views/test/views_test_utils.h"
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

  submenu_item->GetMenuController()->SelectItemAndOpenSubmenu(submenu_item);
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

TEST_F(SubmenuViewTest, BinarySearchTargetForRectAndTooltipMatchesBase) {
  auto owner = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  owner->Show();

  auto menu_delegate = std::make_unique<MenuDelegate>();
  auto root_owning = std::make_unique<MenuItemView>(menu_delegate.get());
  MenuItemView* root = root_owning.get();
  constexpr int kItemCount = 15;
  for (int i = 0; i < kItemCount; ++i) {
    MenuItemView* item = root->AppendMenuItem(i + 1, u"Bookmark Item");
    if (i == 3) {
      item->SetVisible(false);
    }
    if (i == 6) {
      item->SetEnabled(false);
    }
    if (i == 9) {
      root->AppendSeparator();
    }
  }

  MenuRunner menu_runner(std::move(root_owning), 0);
  menu_runner.RunMenuAt(owner.get(), nullptr, gfx::Rect(0, 0, 20, 20),
                        MenuAnchorPosition::kTopLeft,
                        ui::mojom::MenuSourceType::kNone);
  ASSERT_TRUE(menu_runner.IsRunning());

  SubmenuView* submenu = root->GetSubmenu();
  ASSERT_NE(nullptr, submenu);
  ASSERT_GT(submenu->height(), 0);
  ASSERT_GT(submenu->width(), 0);

  const auto verify_queries = [&](bool mirrored) {
    submenu->SetMirrored(mirrored);
    const int x_inside = submenu->width() / 2;
    const int x_left = 0;
    const int x_outside = submenu->width() + 5;
    const int y_limit = submenu->height() + 10;
    // Sweep every row, covering each item edge, padding gap, the
    // hidden/disabled/separator rows, and past the bottom of the submenu.
    for (int y = -5; y <= y_limit; ++y) {
      for (int x : {x_inside, x_left, x_outside}) {
        const gfx::Point pt(x, y);
        const gfx::Rect rect(pt, gfx::Size(1, 1));
        EXPECT_EQ(submenu->ViewTargeterDelegate::TargetForRect(submenu, rect),
                  submenu->GetEventHandlerForPoint(pt))
            << "mirrored=" << mirrored << " pt=" << pt.ToString();
        EXPECT_EQ(submenu->View::GetTooltipHandlerForPoint(pt),
                  submenu->GetTooltipHandlerForPoint(pt))
            << "mirrored=" << mirrored << " pt=" << pt.ToString();
      }
    }
  };

  verify_queries(/*mirrored=*/false);
  verify_queries(/*mirrored=*/true);

  // Mutate child visibility and add a new item, re-layout, and verify again.
  submenu->GetMenuItemAt(3)->SetVisible(true);
  submenu->GetMenuItemAt(4)->SetVisible(false);
  root->AppendMenuItem(kItemCount + 1, u"Appended Item");
  submenu->InvalidateLayout();
  test::RunScheduledLayout(submenu);
  verify_queries(/*mirrored=*/false);

  menu_runner.Cancel();
  owner->CloseNow();
}

}  // namespace views
