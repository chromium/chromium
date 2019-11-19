// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/tabbed_pane.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views {
namespace test {
namespace {

base::string16 DefaultTabTitle() {
  return ASCIIToUTF16("tab");
}

base::string16 GetAccessibleName(View* view) {
  ui::AXNodeData ax_node_data;
  view->GetViewAccessibility().GetAccessibleNodeData(&ax_node_data);
  return ax_node_data.GetString16Attribute(ax::mojom::StringAttribute::kName);
}

ax::mojom::Role GetAccessibleRole(View* view) {
  ui::AXNodeData ax_node_data;
  view->GetViewAccessibility().GetAccessibleNodeData(&ax_node_data);
  return ax_node_data.role;
}

}  // namespace

class TabbedPaneTest : public ViewsTestBase {
 public:
  TabbedPaneTest() = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    tabbed_pane_ = std::make_unique<TabbedPane>();
    tabbed_pane_->set_owned_by_client();

    // Create a widget so that accessibility data will be returned correctly.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->SetContentsView(tabbed_pane_.get());
  }

  void TearDown() override {
    tabbed_pane_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  void MakeTabbedPane(TabbedPane::Orientation orientation,
                      TabbedPane::TabStripStyle style) {
    tabbed_pane_ = std::make_unique<TabbedPane>(orientation, style);
    tabbed_pane_->set_owned_by_client();
  }

  Tab* GetTabAt(size_t index) {
    return static_cast<Tab*>(tabbed_pane_->tab_strip_->children()[index]);
  }

  View* GetSelectedTabContentView() {
    return tabbed_pane_->GetSelectedTabContentView();
  }

  void SendKeyPressToSelectedTab(ui::KeyboardCode keyboard_code) {
    tabbed_pane_->GetSelectedTab()->OnKeyPressed(
        ui::KeyEvent(ui::ET_KEY_PRESSED, keyboard_code,
                     ui::UsLayoutKeyboardCodeToDomCode(keyboard_code), 0));
  }

  std::unique_ptr<Widget> widget_;
  std::unique_ptr<TabbedPane> tabbed_pane_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabbedPaneTest);
};

// Tests tab orientation.
TEST_F(TabbedPaneTest, HorizontalOrientation) {
  EXPECT_EQ(tabbed_pane_->GetOrientation(),
            TabbedPane::Orientation::kHorizontal);
}

// Tests tab orientation.
TEST_F(TabbedPaneTest, VerticalOrientation) {
  MakeTabbedPane(TabbedPane::Orientation::kVertical,
                 TabbedPane::TabStripStyle::kBorder);
  EXPECT_EQ(tabbed_pane_->GetOrientation(), TabbedPane::Orientation::kVertical);
}

// Tests tab strip style.
TEST_F(TabbedPaneTest, TabStripBorderStyle) {
  EXPECT_EQ(tabbed_pane_->GetStyle(), TabbedPane::TabStripStyle::kBorder);
}

// Tests tab strip style.
TEST_F(TabbedPaneTest, TabStripHighlightStyle) {
  MakeTabbedPane(TabbedPane::Orientation::kVertical,
                 TabbedPane::TabStripStyle::kHighlight);
  EXPECT_EQ(tabbed_pane_->GetStyle(), TabbedPane::TabStripStyle::kHighlight);
}

// Tests the preferred size and layout when tabs are aligned horizontally.
// TabbedPane requests a size that fits its largest tab, and disregards the
// width of horizontal tab strips for preferred size calculations. Tab titles
// are elided to fit the actual width.
TEST_F(TabbedPaneTest, SizeAndLayout) {
  View* child1 = tabbed_pane_->AddTab(
      ASCIIToUTF16("tab1 with very long text"),
      std::make_unique<StaticSizedView>(gfx::Size(20, 10)));
  View* child2 =
      tabbed_pane_->AddTab(ASCIIToUTF16("tab2 with very long text"),
                           std::make_unique<StaticSizedView>(gfx::Size(5, 5)));
  tabbed_pane_->SelectTabAt(0);

  // |tabbed_pane_| width should match the largest child in horizontal mode.
  EXPECT_EQ(tabbed_pane_->GetPreferredSize().width(), 20);
  // |tabbed_pane_| reserves extra height for the tab strip in horizontal mode.
  EXPECT_GT(tabbed_pane_->GetPreferredSize().height(), 10);

  // The child views should resize to fit in larger tabbed panes.
  widget_->SetBounds(gfx::Rect(0, 0, 100, 200));
  tabbed_pane_->SetBounds(0, 0, 100, 200);
  RunPendingMessages();
  // |tabbed_pane_| has no border. Therefore the children should be as wide as
  // the |tabbed_pane_|.
  EXPECT_EQ(child1->bounds().width(), 100);
  EXPECT_GT(child1->bounds().height(), 0);
  // |tabbed_pane_| reserves extra height for the tab strip. Therefore the
  // children's height should be smaller than the |tabbed_pane_|'s height.
  EXPECT_LT(child1->bounds().height(), 200);

  // If we switch to the other tab, it should get assigned the same bounds.
  tabbed_pane_->SelectTabAt(1);
  EXPECT_EQ(child1->bounds(), child2->bounds());
}

// Tests the preferred size and layout when tabs are aligned vertically..
TEST_F(TabbedPaneTest, SizeAndLayoutInVerticalOrientation) {
  MakeTabbedPane(TabbedPane::Orientation::kVertical,
                 TabbedPane::TabStripStyle::kBorder);
  View* child1 = tabbed_pane_->AddTab(
      ASCIIToUTF16("tab1"),
      std::make_unique<StaticSizedView>(gfx::Size(20, 10)));
  View* child2 = tabbed_pane_->AddTab(
      ASCIIToUTF16("tab2"), std::make_unique<StaticSizedView>(gfx::Size(5, 5)));
  tabbed_pane_->SelectTabAt(0);

  // |tabbed_pane_| reserves extra width for the tab strip in vertical mode.
  EXPECT_GT(tabbed_pane_->GetPreferredSize().width(), 20);
  // |tabbed_pane_| height should match the largest child in vertical mode.
  EXPECT_EQ(tabbed_pane_->GetPreferredSize().height(), 10);

  // The child views should resize to fit in larger tabbed panes.
  tabbed_pane_->SetBounds(0, 0, 100, 200);
  RunPendingMessages();
  EXPECT_GT(child1->bounds().width(), 0);
  // |tabbed_pane_| reserves extra width for the tab strip. Therefore the
  // children's width should be smaller than the |tabbed_pane_|'s width.
  EXPECT_LT(child1->bounds().width(), 100);
  // |tabbed_pane_| has no border. Therefore the children should be as high as
  // the |tabbed_pane_|.
  EXPECT_EQ(child1->bounds().height(), 200);

  // If we switch to the other tab, it should get assigned the same bounds.
  tabbed_pane_->SelectTabAt(1);
  EXPECT_EQ(child1->bounds(), child2->bounds());
}

TEST_F(TabbedPaneTest, AddAndSelect) {
  // Add several tabs; only the first should be selected automatically.
  for (size_t i = 0; i < 3; ++i) {
    tabbed_pane_->AddTab(DefaultTabTitle(), std::make_unique<View>());
    EXPECT_EQ(i + 1, tabbed_pane_->GetTabCount());
    EXPECT_EQ(0u, tabbed_pane_->GetSelectedTabIndex());
  }

  // Select each tab.
  for (size_t i = 0; i < tabbed_pane_->GetTabCount(); ++i) {
    tabbed_pane_->SelectTabAt(i);
    EXPECT_EQ(i, tabbed_pane_->GetSelectedTabIndex());
  }

  // Add a tab at index 0, it should not be selected automatically.
  View* tab0 = tabbed_pane_->AddTabAtIndex(0, ASCIIToUTF16("tab0"),
                                           std::make_unique<View>());
  EXPECT_NE(tab0, GetSelectedTabContentView());
  EXPECT_NE(0u, tabbed_pane_->GetSelectedTabIndex());
}

TEST_F(TabbedPaneTest, ArrowKeyBindings) {
  // Add several tabs; only the first should be selected automatically.
  for (size_t i = 0; i < 3; ++i) {
    tabbed_pane_->AddTab(DefaultTabTitle(), std::make_unique<View>());
    EXPECT_EQ(i + 1, tabbed_pane_->GetTabCount());
  }

  EXPECT_EQ(0u, tabbed_pane_->GetSelectedTabIndex());

  // Right arrow should select tab 1:
  SendKeyPressToSelectedTab(ui::VKEY_RIGHT);
  EXPECT_EQ(1u, tabbed_pane_->GetSelectedTabIndex());

  // Left arrow should select tab 0:
  SendKeyPressToSelectedTab(ui::VKEY_LEFT);
  EXPECT_EQ(0u, tabbed_pane_->GetSelectedTabIndex());

  // Left arrow again should wrap to tab 2:
  SendKeyPressToSelectedTab(ui::VKEY_LEFT);
  EXPECT_EQ(2u, tabbed_pane_->GetSelectedTabIndex());

  // Right arrow again should wrap to tab 0:
  SendKeyPressToSelectedTab(ui::VKEY_RIGHT);
  EXPECT_EQ(0u, tabbed_pane_->GetSelectedTabIndex());
}

// Use TabbedPane::HandleAccessibleAction() to select tabs and make sure their
// a11y information is correct.
TEST_F(TabbedPaneTest, SelectTabWithAccessibleAction) {
  constexpr size_t kNumTabs = 3;
  for (size_t i = 0; i < kNumTabs; ++i) {
    tabbed_pane_->AddTab(DefaultTabTitle(), std::make_unique<View>());
  }
  // Check the first tab is selected.
  EXPECT_EQ(0u, tabbed_pane_->GetSelectedTabIndex());

  // Check the a11y information for each tab.
  for (size_t i = 0; i < kNumTabs; ++i) {
    ui::AXNodeData data;
    GetTabAt(i)->GetAccessibleNodeData(&data);
    SCOPED_TRACE(testing::Message() << "Tab at index: " << i);
    EXPECT_EQ(ax::mojom::Role::kTab, data.role);
    EXPECT_EQ(DefaultTabTitle(),
              data.GetString16Attribute(ax::mojom::StringAttribute::kName));
    EXPECT_EQ(i == 0,
              data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  }

  ui::AXActionData action;
  action.action = ax::mojom::Action::kSetSelection;
  // Select the first tab.

  GetTabAt(0)->HandleAccessibleAction(action);
  EXPECT_EQ(0u, tabbed_pane_->GetSelectedTabIndex());

  // Select the second tab.
  GetTabAt(1)->HandleAccessibleAction(action);
  EXPECT_EQ(1u, tabbed_pane_->GetSelectedTabIndex());
  // Select the second tab again.
  GetTabAt(1)->HandleAccessibleAction(action);
  EXPECT_EQ(1u, tabbed_pane_->GetSelectedTabIndex());
}

TEST_F(TabbedPaneTest, AccessiblePaneTitleTracksActiveTabTitle) {
  const base::string16 kFirstTitle = ASCIIToUTF16("Tab1");
  const base::string16 kSecondTitle = ASCIIToUTF16("Tab2");
  tabbed_pane_->AddTab(kFirstTitle, std::make_unique<View>());
  tabbed_pane_->AddTab(kSecondTitle, std::make_unique<View>());
  EXPECT_EQ(kFirstTitle, GetAccessibleName(tabbed_pane_.get()));
  tabbed_pane_->SelectTabAt(1);
  EXPECT_EQ(kSecondTitle, GetAccessibleName(tabbed_pane_.get()));
}

TEST_F(TabbedPaneTest, AccessiblePaneContentsTitleTracksTabTitle) {
  const base::string16 kFirstTitle = ASCIIToUTF16("Tab1");
  const base::string16 kSecondTitle = ASCIIToUTF16("Tab2");
  View* const tab1_contents =
      tabbed_pane_->AddTab(kFirstTitle, std::make_unique<View>());
  View* const tab2_contents =
      tabbed_pane_->AddTab(kSecondTitle, std::make_unique<View>());
  EXPECT_EQ(kFirstTitle, GetAccessibleName(tab1_contents));
  EXPECT_EQ(kSecondTitle, GetAccessibleName(tab2_contents));
}

TEST_F(TabbedPaneTest, AccessiblePaneContentsRoleIsTab) {
  const base::string16 kFirstTitle = ASCIIToUTF16("Tab1");
  const base::string16 kSecondTitle = ASCIIToUTF16("Tab2");
  View* const tab1_contents =
      tabbed_pane_->AddTab(kFirstTitle, std::make_unique<View>());
  View* const tab2_contents =
      tabbed_pane_->AddTab(kSecondTitle, std::make_unique<View>());
  EXPECT_EQ(ax::mojom::Role::kTab, GetAccessibleRole(tab1_contents));
  EXPECT_EQ(ax::mojom::Role::kTab, GetAccessibleRole(tab2_contents));
}

}  // namespace test
}  // namespace views
