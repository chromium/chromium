// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/tabbed_pane.h"

#include <memory>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views::test {
namespace {

std::u16string DefaultTabTitle() {
  return u"tab";
}

std::u16string GetAccessibleName(View* view) {
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

using TabbedPaneTest = ViewsTestBase;

// Tests tab orientation.
TEST_F(TabbedPaneTest, HorizontalOrientationDefault) {
  auto tabbed_pane = std::make_unique<TabbedPane>();
  EXPECT_EQ(tabbed_pane->GetOrientation(),
            TabbedPane::Orientation::kHorizontal);
}

// Tests tab orientation.
TEST_F(TabbedPaneTest, VerticalOrientation) {
  auto tabbed_pane = std::make_unique<TabbedPane>(
      TabbedPane::Orientation::kVertical, TabbedPane::TabStripStyle::kBorder);
  EXPECT_EQ(tabbed_pane->GetOrientation(), TabbedPane::Orientation::kVertical);
}

// Tests tab strip style.
TEST_F(TabbedPaneTest, TabStripBorderStyle) {
  auto tabbed_pane = std::make_unique<TabbedPane>();
  EXPECT_EQ(tabbed_pane->GetStyle(), TabbedPane::TabStripStyle::kBorder);
}

// Tests tab strip style.
TEST_F(TabbedPaneTest, TabStripHighlightStyle) {
  auto tabbed_pane =
      std::make_unique<TabbedPane>(TabbedPane::Orientation::kVertical,
                                   TabbedPane::TabStripStyle::kHighlight);
  EXPECT_EQ(tabbed_pane->GetStyle(), TabbedPane::TabStripStyle::kHighlight);
}

TEST_F(TabbedPaneTest, ScrollingDisabled) {
  auto tabbed_pane = std::make_unique<TabbedPane>(
      TabbedPane::Orientation::kVertical, TabbedPane::TabStripStyle::kBorder);
  EXPECT_EQ(tabbed_pane->GetScrollView(), nullptr);
}

TEST_F(TabbedPaneTest, ScrollingEnabled) {
  auto tabbed_pane_vertical =
      std::make_unique<TabbedPane>(TabbedPane::Orientation::kVertical,
                                   TabbedPane::TabStripStyle::kBorder, true);
  ASSERT_NE(tabbed_pane_vertical->GetScrollView(), nullptr);
  EXPECT_THAT(tabbed_pane_vertical->GetScrollView(), testing::A<ScrollView*>());

  auto tabbed_pane_horizontal =
      std::make_unique<TabbedPane>(TabbedPane::Orientation::kHorizontal,
                                   TabbedPane::TabStripStyle::kBorder, true);
  ASSERT_NE(tabbed_pane_horizontal->GetScrollView(), nullptr);
  EXPECT_THAT(tabbed_pane_horizontal->GetScrollView(),
              testing::A<ScrollView*>());
}

// Tests the preferred size and layout when tabs are aligned vertically..
TEST_F(TabbedPaneTest, SizeAndLayoutInVerticalOrientation) {
  auto tabbed_pane = std::make_unique<TabbedPane>(
      TabbedPane::Orientation::kVertical, TabbedPane::TabStripStyle::kBorder);
  View* child1 = tabbed_pane->AddTab(
      u"tab1", std::make_unique<StaticSizedView>(gfx::Size(20, 10)));
  View* child2 = tabbed_pane->AddTab(
      u"tab2", std::make_unique<StaticSizedView>(gfx::Size(5, 5)));
  tabbed_pane->SelectTabAt(0);

  // |tabbed_pane_| reserves extra width for the tab strip in vertical mode.
  EXPECT_GT(tabbed_pane->GetPreferredSize({}).width(), 20);
  // |tabbed_pane_| height should match the largest child in vertical mode.
  EXPECT_EQ(tabbed_pane->GetPreferredSize({}).height(), 10);

  // The child views should resize to fit in larger tabbed panes.
  tabbed_pane->SetBounds(0, 0, 100, 200);

  EXPECT_GT(child1->bounds().width(), 0);
  // |tabbed_pane_| reserves extra width for the tab strip. Therefore the
  // children's width should be smaller than the |tabbed_pane_|'s width.
  EXPECT_LT(child1->bounds().width(), 100);
  // |tabbed_pane_| has no border. Therefore the children should be as high as
  // the |tabbed_pane_|.
  EXPECT_EQ(child1->bounds().height(), 200);

  // If we switch to the other tab, it should get assigned the same bounds.
  tabbed_pane->SelectTabAt(1);
  EXPECT_EQ(child1->bounds(), child2->bounds());
}

TEST_F(TabbedPaneTest, AccessibleAttributes) {
  auto tabbed_pane = std::make_unique<TabbedPane>();

  ui::AXNodeData data;
  tabbed_pane->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kTabList);
}

class TabbedPaneWithWidgetTest : public ViewsTestBase {
 public:
  TabbedPaneWithWidgetTest() = default;

  TabbedPaneWithWidgetTest(const TabbedPaneWithWidgetTest&) = delete;
  TabbedPaneWithWidgetTest& operator=(const TabbedPaneWithWidgetTest&) = delete;

  void SetUp() override {
    ViewsTestBase::SetUp();
    auto tabbed_pane = std::make_unique<TabbedPane>();

    // Create a widget so that accessibility data will be returned correctly.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    tabbed_pane_ = tabbed_pane.get();
    widget_->SetContentsView(std::move(tabbed_pane));
  }

  void TearDown() override {
    tabbed_pane_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  TabbedPaneTab* GetTabAt(size_t index) {
    return static_cast<TabbedPaneTab*>(
        tabbed_pane_->tab_strip_->children()[index]);
  }

  View* GetSelectedTabContentView() {
    return tabbed_pane_->GetSelectedTabContentView();
  }

  void SendKeyPressToSelectedTab(ui::KeyboardCode keyboard_code) {
    tabbed_pane_->GetSelectedTab()->OnKeyPressed(
        ui::KeyEvent(ui::EventType::kKeyPressed, keyboard_code,
                     ui::UsLayoutKeyboardCodeToDomCode(keyboard_code), 0));
  }

  std::unique_ptr<Widget> widget_;
  raw_ptr<TabbedPane> tabbed_pane_;
};

// Tests the preferred size and layout when tabs are aligned horizontally.
// TabbedPane requests a size that fits the largest child or the minimum size
// necessary to display the tab titles, whichever is larger.
TEST_F(TabbedPaneWithWidgetTest, SizeAndLayout) {
  View* child1 = tabbed_pane_->AddTab(
      u"tab1", std::make_unique<StaticSizedView>(gfx::Size(20, 10)));
  View* child2 = tabbed_pane_->AddTab(
      u"tab2", std::make_unique<StaticSizedView>(gfx::Size(5, 5)));
  tabbed_pane_->SelectTabAt(0);

  // In horizontal mode, |tabbed_pane_| width should match the largest child or
  // the minimum size necessary to display the tab titles, whichever is larger.
  EXPECT_EQ(tabbed_pane_->GetPreferredSize({}).width(),
            tabbed_pane_->GetTabAt(0)->GetPreferredSize({}).width() +
                tabbed_pane_->GetTabAt(1)->GetPreferredSize({}).width());
  // |tabbed_pane_| reserves extra height for the tab strip in horizontal mode.
  EXPECT_GT(tabbed_pane_->GetPreferredSize({}).height(), 10);

  // Test that the preferred size is now the size of the size of the largest
  // child.
  View* child3 = tabbed_pane_->AddTab(
      u"tab3", std::make_unique<StaticSizedView>(gfx::Size(150, 5)));
  EXPECT_EQ(tabbed_pane_->GetPreferredSize({}).width(), 150);

  // The child views should resize to fit in larger tabbed panes.
  widget_->SetBounds(gfx::Rect(0, 0, 300, 200));
  tabbed_pane_->SetBounds(0, 0, 300, 200);
  RunPendingMessages();
  // |tabbed_pane_| has no border. Therefore the children should be as wide as
  // the |tabbed_pane_|.
  EXPECT_EQ(child1->bounds().width(), 300);
  EXPECT_GT(child1->bounds().height(), 0);
  // |tabbed_pane_| reserves extra height for the tab strip. Therefore the
  // children's height should be smaller than the |tabbed_pane_|'s height.
  EXPECT_LT(child1->bounds().height(), 200);

  // If we switch to the other tab, it should get assigned the same bounds.
  tabbed_pane_->SelectTabAt(1);
  EXPECT_EQ(child1->bounds(), child2->bounds());
  EXPECT_EQ(child2->bounds(), child3->bounds());
}

TEST_F(TabbedPaneWithWidgetTest, AddAndSelect) {
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
  View* tab0 =
      tabbed_pane_->AddTabAtIndex(0, u"tab0", std::make_unique<View>());
  EXPECT_NE(tab0, GetSelectedTabContentView());
  EXPECT_NE(0u, tabbed_pane_->GetSelectedTabIndex());
}

TEST_F(TabbedPaneWithWidgetTest, ArrowKeyBindings) {
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

TEST_F(TabbedPaneWithWidgetTest, ArrowKeyBindingsWithRTL) {
  // Add several tabs; only the first should be selected automatically.
  base::i18n::SetRTLForTesting(true);
  EXPECT_TRUE(base::i18n::IsRTL());
  for (size_t i = 0; i < 3; ++i) {
    tabbed_pane_->AddTab(DefaultTabTitle(), std::make_unique<View>());
    EXPECT_EQ(i + 1, tabbed_pane_->GetTabCount());
  }

  EXPECT_EQ(0u, tabbed_pane_->GetSelectedTabIndex());

  // Left arrow should select tab 1:
  SendKeyPressToSelectedTab(ui::VKEY_LEFT);
  EXPECT_EQ(1u, tabbed_pane_->GetSelectedTabIndex());

  // Left arrow should select tab 2:
  SendKeyPressToSelectedTab(ui::VKEY_LEFT);
  EXPECT_EQ(2u, tabbed_pane_->GetSelectedTabIndex());

  // Left arrow again should wrap to tab 0:
  SendKeyPressToSelectedTab(ui::VKEY_LEFT);
  EXPECT_EQ(0u, tabbed_pane_->GetSelectedTabIndex());

  // Right arrow again should wrap to tab 2:
  SendKeyPressToSelectedTab(ui::VKEY_RIGHT);
  EXPECT_EQ(2u, tabbed_pane_->GetSelectedTabIndex());

  // Right arrow again should wrap to tab 1:
  SendKeyPressToSelectedTab(ui::VKEY_RIGHT);
  EXPECT_EQ(1u, tabbed_pane_->GetSelectedTabIndex());

  // Right arrow again should wrap to tab 0:
  SendKeyPressToSelectedTab(ui::VKEY_RIGHT);
  EXPECT_EQ(0u, tabbed_pane_->GetSelectedTabIndex());

  base::i18n::SetRTLForTesting(false);
}

// Use TabbedPane::HandleAccessibleAction() to select tabs and make sure their
// a11y information is correct.
TEST_F(TabbedPaneWithWidgetTest, SelectTabWithAccessibleAction) {
  constexpr size_t kNumTabs = 3;
  for (size_t i = 0; i < kNumTabs; ++i) {
    tabbed_pane_->AddTab(DefaultTabTitle(), std::make_unique<View>());
  }
  // Check the first tab is selected.
  EXPECT_EQ(0u, tabbed_pane_->GetSelectedTabIndex());

  // Check the a11y information for each tab.
  for (size_t i = 0; i < kNumTabs; ++i) {
    ui::AXNodeData data;
    GetTabAt(i)->GetViewAccessibility().GetAccessibleNodeData(&data);
    SCOPED_TRACE(testing::Message() << "TabbedPaneTab at index: " << i);
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

TEST_F(TabbedPaneWithWidgetTest, AccessiblePaneTitleTracksActiveTabTitle) {
  const std::u16string kFirstTitle = u"Tab1";
  const std::u16string kSecondTitle = u"Tab2";
  tabbed_pane_->AddTab(kFirstTitle, std::make_unique<View>());
  tabbed_pane_->AddTab(kSecondTitle, std::make_unique<View>());
  EXPECT_EQ(kFirstTitle, GetAccessibleName(tabbed_pane_));
  tabbed_pane_->SelectTabAt(1);
  EXPECT_EQ(kSecondTitle, GetAccessibleName(tabbed_pane_));
}

TEST_F(TabbedPaneWithWidgetTest, AccessiblePaneContentsTitleTracksTabTitle) {
  const std::u16string kFirstTitle = u"Tab1";
  const std::u16string kSecondTitle = u"Tab2";
  View* const tab1_contents =
      tabbed_pane_->AddTab(kFirstTitle, std::make_unique<View>());
  View* const tab2_contents =
      tabbed_pane_->AddTab(kSecondTitle, std::make_unique<View>());
  EXPECT_EQ(kFirstTitle, GetAccessibleName(tab1_contents));
  EXPECT_EQ(kSecondTitle, GetAccessibleName(tab2_contents));
}

TEST_F(TabbedPaneWithWidgetTest, AccessiblePaneContentsRoleIsTabPanel) {
  const std::u16string kFirstTitle = u"Tab1";
  const std::u16string kSecondTitle = u"Tab2";
  View* const tab1_contents =
      tabbed_pane_->AddTab(kFirstTitle, std::make_unique<View>());
  View* const tab2_contents =
      tabbed_pane_->AddTab(kSecondTitle, std::make_unique<View>());
  EXPECT_EQ(ax::mojom::Role::kTabPanel, GetAccessibleRole(tab1_contents));
  EXPECT_EQ(ax::mojom::Role::kTabPanel, GetAccessibleRole(tab2_contents));
}

TEST_F(TabbedPaneWithWidgetTest, AccessibleEvents) {
  tabbed_pane_->AddTab(u"Tab1", std::make_unique<View>());
  tabbed_pane_->AddTab(u"Tab2", std::make_unique<View>());
  test::AXEventCounter counter(views::AXEventManager::Get());

  // This is needed for FocusManager::SetFocusedViewWithReason to notify
  // observers observers of focus changes.
  if (widget_ && !widget_->IsActive())
    widget_->Activate();

  EXPECT_EQ(0u, tabbed_pane_->GetSelectedTabIndex());

  // Change the selected tab without giving the tab focus should result in a
  // selection change for the new tab and a selected-children-changed for the
  // tab list. No focus events should occur.
  tabbed_pane_->SelectTabAt(1);
  EXPECT_EQ(1u, tabbed_pane_->GetSelectedTabIndex());
  EXPECT_EQ(
      1, counter.GetCount(ax::mojom::Event::kSelection, ax::mojom::Role::kTab));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kSelectedChildrenChanged,
                                ax::mojom::Role::kTabList));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kFocus));

  counter.ResetAllCounts();

  // Focusing the selected tab should only result in a focus event for that tab.
  tabbed_pane_->GetFocusManager()->SetFocusedView(tabbed_pane_->GetTabAt(1));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kFocus));
  EXPECT_EQ(1,
            counter.GetCount(ax::mojom::Event::kFocus, ax::mojom::Role::kTab));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kSelection));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kSelectedChildrenChanged));

  counter.ResetAllCounts();

  // Arrowing left to the first tab selects it. Therefore we should get the same
  // events as we did when SelectTabAt() was called.
  SendKeyPressToSelectedTab(ui::VKEY_LEFT);
  EXPECT_EQ(0u, tabbed_pane_->GetSelectedTabIndex());
  EXPECT_EQ(
      1, counter.GetCount(ax::mojom::Event::kSelection, ax::mojom::Role::kTab));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kSelectedChildrenChanged,
                                ax::mojom::Role::kTabList));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kFocus));

  counter.ResetAllCounts();

  // Focusing an unselected tab, if the UI allows it, a should only result in a
  // focus event for that tab.
  tabbed_pane_->GetFocusManager()->SetFocusedView(tabbed_pane_->GetTabAt(1));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kFocus));
  EXPECT_EQ(1,
            counter.GetCount(ax::mojom::Event::kFocus, ax::mojom::Role::kTab));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kSelection));
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kSelectedChildrenChanged));
}

TEST_F(TabbedPaneWithWidgetTest, AccessibleNameTest) {
  tabbed_pane_->AddTab(u"Tab1", std::make_unique<View>());
  ui::AXNodeData data;

  GetTabAt(0)->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(u"Tab1",
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(ax::mojom::NameFrom::kContents, data.GetNameFrom());

  GetTabAt(0)->SetTitleText(u"Updated Tab1");
  data = ui::AXNodeData();
  GetTabAt(0)->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(u"Updated Tab1",
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(ax::mojom::NameFrom::kContents, data.GetNameFrom());

  GetTabAt(0)->SetTitleText(u"");
  data = ui::AXNodeData();
  GetTabAt(0)->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(u"", data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(ax::mojom::NameFrom::kAttributeExplicitlyEmpty, data.GetNameFrom());
}

TEST_F(TabbedPaneWithWidgetTest, AccessibleSelected) {
  tabbed_pane_->AddTab(u"Tab1", std::make_unique<View>());
  ui::AXNodeData data;

  GetTabAt(0)->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  data = ui::AXNodeData();
  GetTabAt(0)->SetSelected(false);
  GetTabAt(0)->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  data = ui::AXNodeData();
  GetTabAt(0)->SetSelected(true);
  GetTabAt(0)->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

}  // namespace views::test
