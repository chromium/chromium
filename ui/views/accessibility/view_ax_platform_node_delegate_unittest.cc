// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/base/models/table_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/accessibility/ax_widget_obj_wrapper.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/menu/test_menu_item_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace views::test {

namespace {

class TestButton : public Button {
 public:
  TestButton() : Button(Button::PressedCallback()) {}
  TestButton(const TestButton&) = delete;
  TestButton& operator=(const TestButton&) = delete;
  ~TestButton() override = default;
};

class TestAXEventObserver : public AXEventObserver {
 public:
  explicit TestAXEventObserver(AXAuraObjCache* cache) : cache_(cache) {
    AXEventManager::Get()->AddObserver(this);
  }
  TestAXEventObserver(const TestAXEventObserver&) = delete;
  TestAXEventObserver& operator=(const TestAXEventObserver&) = delete;
  ~TestAXEventObserver() override {
    AXEventManager::Get()->RemoveObserver(this);
  }

  // AXEventObserver:
  void OnViewEvent(View* view, ax::mojom::Event event_type) override {
    std::vector<AXAuraObjWrapper*> out_children;
    AXAuraObjWrapper* ax_obj = cache_->GetOrCreate(view->GetWidget());
    ax_obj->GetChildren(&out_children);
  }

 private:
  raw_ptr<AXAuraObjCache> cache_;
};

}  // namespace

class TestTableModel : public ui::TableModel {
 public:
  TestTableModel() = default;

  TestTableModel(const TestTableModel&) = delete;
  TestTableModel& operator=(const TestTableModel&) = delete;

  // ui::TableModel:
  size_t RowCount() override { return 10; }

  std::u16string GetText(size_t row, int column_id) override {
    const char* const cells[5][4] = {
        {"Orange", "Orange", "South america", "$5"},
        {"Apple", "Green", "Canada", "$3"},
        {"Blue berries", "Blue", "Mexico", "$10.3"},
        {"Strawberries", "Red", "California", "$7"},
        {"Cantaloupe", "Orange", "South america", "$5"},
    };

    return base::ASCIIToUTF16(cells[row % 5][column_id]);
  }

  void SetObserver(ui::TableModelObserver* observer) override {}
};

class ViewAXPlatformNodeDelegateTest : public ViewsTestBase {
 public:
  ViewAXPlatformNodeDelegateTest() : ax_mode_setter_(ui::kAXModeComplete) {}
  ViewAXPlatformNodeDelegateTest(const ViewAXPlatformNodeDelegateTest&) =
      delete;
  ViewAXPlatformNodeDelegateTest& operator=(
      const ViewAXPlatformNodeDelegateTest&) = delete;
  ~ViewAXPlatformNodeDelegateTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = new Widget;
    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(std::move(params));

    button_ = new TestButton();
    button_->SetID(NON_DEFAULT_VIEW_ID);
    button_->SetSize(gfx::Size(20, 20));

    label_ = new Label();
    label_->SetID(DEFAULT_VIEW_ID);
    button_->AddChildView(label_.get());

    widget_->GetRootView()->AddChildView(button_.get());
    widget_->Show();
  }

  void TearDown() override {
    if (!widget_->IsClosed())
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  ViewAXPlatformNodeDelegate* button_accessibility() {
    return static_cast<ViewAXPlatformNodeDelegate*>(
        &button_->GetViewAccessibility());
  }

  ViewAXPlatformNodeDelegate* label_accessibility() {
    return static_cast<ViewAXPlatformNodeDelegate*>(
        &label_->GetViewAccessibility());
  }

  ViewAXPlatformNodeDelegate* view_accessibility(View* view) {
    return static_cast<ViewAXPlatformNodeDelegate*>(
        &view->GetViewAccessibility());
  }

  bool SetFocused(ViewAXPlatformNodeDelegate* ax_delegate, bool focused) {
    ui::AXActionData data;
    data.action =
        focused ? ax::mojom::Action::kFocus : ax::mojom::Action::kBlur;
    return ax_delegate->AccessibilityPerformAction(data);
  }

  // Sets up a more complicated structure of Views - one parent View with four
  // child Views.
  View::Views SetUpExtraViews() {
    View* parent_view =
        widget_->GetRootView()->AddChildView(std::make_unique<View>());
    View::Views views{parent_view};
    for (int i = 0; i < 4; i++)
      views.push_back(parent_view->AddChildView(std::make_unique<View>()));
    return views;
  }

  // Adds group id information to the first 5 values in |views|.
  void SetUpExtraViewsGroups(const View::Views& views) {
    //                v[0] g1
    //     |        |        |      |
    // v[1] g1  v[2] g1  v[3] g2  v[4]
    ASSERT_GE(views.size(), 5u);

    views[0]->SetGroup(1);
    views[1]->SetGroup(1);
    views[2]->SetGroup(1);
    views[3]->SetGroup(2);
    // Skip views[4] - no group id.
  }

  // Adds posInSet and setSize overrides to the first 5 values in |views|.
  void SetUpExtraViewsSetOverrides(const View::Views& views) {
    //                     v[0] p4 s4
    //      |            |            |            |
    //  v[1] p3 s4   v[2] p2 s4   v[3] p- s-   v[4] p1 s4
    ASSERT_GE(views.size(), 5u);

    views[0]->GetViewAccessibility().OverridePosInSet(4, 4);
    views[1]->GetViewAccessibility().OverridePosInSet(3, 4);
    views[2]->GetViewAccessibility().OverridePosInSet(2, 4);
    // Skip views[3] - no override.
    views[4]->GetViewAccessibility().OverridePosInSet(1, 4);
  }

 protected:
  const int DEFAULT_VIEW_ID = 0;
  const int NON_DEFAULT_VIEW_ID = 1;

  raw_ptr<Widget> widget_ = nullptr;
  raw_ptr<Button> button_ = nullptr;
  raw_ptr<Label> label_ = nullptr;
  ScopedAXModeSetter ax_mode_setter_;
};

class ViewAXPlatformNodeDelegateTableTest
    : public ViewAXPlatformNodeDelegateTest {
 public:
  void SetUp() override {
    ViewAXPlatformNodeDelegateTest::SetUp();

    std::vector<ui::TableColumn> columns;
    columns.push_back(TestTableColumn(0, "Fruit"));
    columns.push_back(TestTableColumn(1, "Color"));
    columns.push_back(TestTableColumn(2, "Origin"));
    columns.push_back(TestTableColumn(3, "Price"));

    model_ = std::make_unique<TestTableModel>();
    auto table =
        std::make_unique<TableView>(model_.get(), columns, TEXT_ONLY, true);
    table_ = table.get();
    widget_->GetRootView()->AddChildView(
        TableView::CreateScrollViewWithTable(std::move(table)));
  }

  ui::TableColumn TestTableColumn(int id, const std::string& title) {
    ui::TableColumn column;
    column.id = id;
    column.title = base::ASCIIToUTF16(title.c_str());
    column.sortable = true;
    return column;
  }

  ViewAXPlatformNodeDelegate* table_accessibility() {
    return view_accessibility(table_);
  }

 private:
  std::unique_ptr<TestTableModel> model_;
  raw_ptr<TableView> table_ = nullptr;  // Owned by parent.
};

class ViewAXPlatformNodeDelegateMenuTest
    : public ViewAXPlatformNodeDelegateTest {
 public:
  void SetUp() override {
    ViewAXPlatformNodeDelegateTest::SetUp();

    owner_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    owner_->Init(std::move(params));
    owner_->Show();

    menu_delegate_ = std::make_unique<TestMenuDelegate>();
    menu_ = new views::TestMenuItemView(menu_delegate_.get());
    runner_ = std::make_unique<MenuRunner>(menu_, 0);

    menu_->AppendMenuItemImpl(0, u"normal", ui::ImageModel(),
                              MenuItemView::Type::kNormal);
    menu_->AppendMenuItemImpl(1, u"submenu", ui::ImageModel(),
                              MenuItemView::Type::kSubMenu);
    menu_->AppendMenuItemImpl(2, u"actionable", ui::ImageModel(),
                              MenuItemView::Type::kActionableSubMenu);
    menu_->AppendMenuItemImpl(3, u"checkbox", ui::ImageModel(),
                              MenuItemView::Type::kCheckbox);
    menu_->AppendMenuItemImpl(4, u"radio", ui::ImageModel(),
                              MenuItemView::Type::kRadio);
    menu_->AppendMenuItemImpl(5, u"separator", ui::ImageModel(),
                              MenuItemView::Type::kSeparator);
    menu_->AppendMenuItemImpl(6, u"highlighted", ui::ImageModel(),
                              MenuItemView::Type::kHighlighted);
    menu_->AppendMenuItemImpl(7, u"title", ui::ImageModel(),
                              MenuItemView::Type::kTitle);

    submenu_ = menu_->GetSubmenu();
    submenu_->GetMenuItemAt(3)->SetSelected(true);
  }

  void TearDown() override {
    if (owner_)
      owner_->CloseNow();
    ViewAXPlatformNodeDelegateTest::TearDown();
  }

  void RunMenu() {
    runner_.get()->RunMenuAt(owner_.get(), nullptr, gfx::Rect(),
                             MenuAnchorPosition::kTopLeft,
                             ui::MENU_SOURCE_NONE);
  }

  ViewAXPlatformNodeDelegate* submenu_accessibility() {
    return view_accessibility(submenu_);
  }

 private:
  // Owned by runner_.
  raw_ptr<views::TestMenuItemView> menu_ = nullptr;

  raw_ptr<SubmenuView> submenu_ = nullptr;
  std::unique_ptr<TestMenuDelegate> menu_delegate_;
  std::unique_ptr<MenuRunner> runner_;
  UniqueWidgetPtr owner_;
};

TEST_F(ViewAXPlatformNodeDelegateTest, FocusBehaviorShouldAffectIgnoredState) {
  EXPECT_EQ(ax::mojom::Role::kButton, button_accessibility()->GetRole());
  EXPECT_FALSE(button_accessibility()->HasState(ax::mojom::State::kIgnored));

  // Since the label is a subview of |button_|, and the button is keyboard
  // focusable, the label is assumed to form part of the button and should be
  // ignored.
  EXPECT_EQ(ax::mojom::Role::kStaticText, label_accessibility()->GetRole());
  EXPECT_TRUE(label_accessibility()->HasState(ax::mojom::State::kIgnored));

  // This will happen for all potentially keyboard-focusable Views with
  // non-keyboard-focusable children, so if we make the button unfocusable, the
  // label will not be ignored any more.
  button_->SetFocusBehavior(View::FocusBehavior::NEVER);

  EXPECT_EQ(ax::mojom::Role::kButton, button_accessibility()->GetRole());
  EXPECT_FALSE(button_accessibility()->HasState(ax::mojom::State::kIgnored));
  EXPECT_EQ(ax::mojom::Role::kStaticText, label_accessibility()->GetRole());
  EXPECT_FALSE(label_accessibility()->HasState(ax::mojom::State::kIgnored));
}

TEST_F(ViewAXPlatformNodeDelegateTest, BoundsShouldMatch) {
  gfx::Rect bounds = gfx::ToEnclosingRect(
      button_accessibility()->GetData().relative_bounds.bounds);
  gfx::Rect screen_bounds =
      button_accessibility()->GetUnclippedScreenBoundsRect();

  EXPECT_EQ(button_->GetBoundsInScreen(), bounds);
  EXPECT_EQ(screen_bounds, bounds);
}

TEST_F(ViewAXPlatformNodeDelegateTest, LabelIsChildOfButton) {
  // Disable focus rings for this test: they introduce extra children that can
  // be either before or after the label, which complicates correctness testing.
  button_->SetInstallFocusRingOnFocus(false);

  // Since the label is a subview of |button_|, and the button is keyboard
  // focusable, the label is assumed to form part of the button and should be
  // ignored, i.e. not visible in the accessibility tree that is available to
  // platform APIs.
  EXPECT_NE(View::FocusBehavior::NEVER, button_->GetFocusBehavior());
  EXPECT_EQ(0u, button_accessibility()->GetChildCount());
  EXPECT_EQ(ax::mojom::Role::kStaticText, label_accessibility()->GetRole());

  // Modify the focus behavior to make the button unfocusable, and verify that
  // the label is now a child of the button.
  button_->SetFocusBehavior(View::FocusBehavior::NEVER);
  EXPECT_EQ(1u, button_accessibility()->GetChildCount());
  EXPECT_EQ(label_->GetNativeViewAccessible(),
            button_accessibility()->ChildAtIndex(0));
  EXPECT_EQ(button_->GetNativeViewAccessible(),
            label_accessibility()->GetParent());
  EXPECT_EQ(ax::mojom::Role::kStaticText, label_accessibility()->GetRole());
}

// Verify Views with invisible ancestors have ax::mojom::State::kInvisible.
TEST_F(ViewAXPlatformNodeDelegateTest, InvisibleViews) {
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(button_accessibility()->HasState(ax::mojom::State::kInvisible));
  EXPECT_FALSE(label_accessibility()->HasState(ax::mojom::State::kInvisible));
  button_->SetVisible(false);
  EXPECT_TRUE(button_accessibility()->HasState(ax::mojom::State::kInvisible));
  EXPECT_TRUE(label_accessibility()->HasState(ax::mojom::State::kInvisible));
}

TEST_F(ViewAXPlatformNodeDelegateTest, SetFocus) {
  // Make |button_| focusable, and focus/unfocus it via
  // ViewAXPlatformNodeDelegate.
  button_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  EXPECT_EQ(nullptr, button_->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(nullptr, button_accessibility()->GetFocus());
  EXPECT_TRUE(SetFocused(button_accessibility(), true));
  EXPECT_EQ(button_, button_->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(button_->GetNativeViewAccessible(),
            button_accessibility()->GetFocus());
  EXPECT_TRUE(SetFocused(button_accessibility(), false));
  EXPECT_EQ(nullptr, button_->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(nullptr, button_accessibility()->GetFocus());

  // If the button is not focusable at all, or if it is disabled for
  // accessibility, SetFocused() should return false.
  button_->SetEnabled(false);
  EXPECT_FALSE(SetFocused(button_accessibility(), true));
  button_->SetEnabled(true);

  button_accessibility()->OverrideIsEnabled(false);
  EXPECT_FALSE(SetFocused(button_accessibility(), true));

  EXPECT_FALSE(button_accessibility()->IsAccessibilityFocusable());
  button_accessibility()->OverrideIsEnabled(true);
  EXPECT_TRUE(button_accessibility()->IsAccessibilityFocusable());
}

TEST_F(ViewAXPlatformNodeDelegateTest, GetAuthorUniqueIdDefault) {
  ASSERT_EQ(u"", label_accessibility()->GetAuthorUniqueId());
}

TEST_F(ViewAXPlatformNodeDelegateTest, GetAuthorUniqueIdNonDefault) {
  ASSERT_EQ(u"view_1", button_accessibility()->GetAuthorUniqueId());
}

TEST_F(ViewAXPlatformNodeDelegateTest, OverrideNameAndDescription) {
  // Initially the button has no name and no description.
  EXPECT_EQ(button_accessibility()->GetName(), "");
  EXPECT_EQ(button_accessibility()->GetNameFrom(), ax::mojom::NameFrom::kNone);
  EXPECT_EQ(button_accessibility()->GetDescription(), "");
  EXPECT_EQ(button_accessibility()->GetDescriptionFrom(),
            ax::mojom::DescriptionFrom::kNone);

  // Setting the name to the empty string without explicitly setting the
  // source to reflect that should trigger a DCHECK in OverrideName.
  EXPECT_DCHECK_DEATH_WITH(button_accessibility()->OverrideName(""),
                           "Check failed: name.empty\\(\\) == name_from == "
                           "ax::mojom::NameFrom::kAttributeExplicitlyEmpty");

  // Setting the name to a non-empty string with a NameFrom of
  // kAttributeExplicitlyEmpty should trigger a DCHECK in OverrideName.
  EXPECT_DCHECK_DEATH_WITH(
      button_accessibility()->OverrideName(
          "foo", ax::mojom::NameFrom::kAttributeExplicitlyEmpty),
      "Check failed: name.empty\\(\\) == name_from == "
      "ax::mojom::NameFrom::kAttributeExplicitlyEmpty");

  button_accessibility()->OverrideName(
      "", ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  EXPECT_EQ(button_accessibility()->GetName(), "");
  EXPECT_EQ(button_accessibility()->GetNameFrom(),
            ax::mojom::NameFrom::kAttributeExplicitlyEmpty);

  // Setting the description to the empty string without explicitly setting
  // the source to reflect that should trigger a DCHECK in OverrideDescription.
  EXPECT_DCHECK_DEATH_WITH(
      button_accessibility()->OverrideDescription(""),
      "Check failed: description.empty\\(\\) == description_from == "
      "ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty");

  // Setting the description to a non-empty string with a DescriptionFrom of
  // kAttributeExplicitlyEmpty should trigger a DCHECK in OverrideDescription.
  EXPECT_DCHECK_DEATH_WITH(
      button_accessibility()->OverrideDescription(
          "foo", ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty),
      "Check failed: description.empty\\(\\) == description_from == "
      "ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty");

  button_accessibility()->OverrideDescription(
      "", ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  EXPECT_EQ(button_accessibility()->GetDescription(), "");
  EXPECT_EQ(button_accessibility()->GetDescriptionFrom(),
            ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);

  // Overriding the name and description without specifying the sources
  // should set the sources to kAttribute and kAriaDescription respectively.
  button_accessibility()->OverrideName("Button's Name");
  EXPECT_EQ(button_accessibility()->GetName(), "Button's Name");
  EXPECT_EQ(button_accessibility()->GetNameFrom(),
            ax::mojom::NameFrom::kAttribute);

  button_accessibility()->OverrideDescription("Button's description");
  EXPECT_EQ(button_accessibility()->GetDescription(), "Button's description");
  EXPECT_EQ(button_accessibility()->GetDescriptionFrom(),
            ax::mojom::DescriptionFrom::kAriaDescription);

  // Initially the label has no name and no description.
  EXPECT_EQ(label_accessibility()->GetName(), "");
  EXPECT_EQ(label_accessibility()->GetDescription(), "");

  // Set the name and description of the label using other source types
  // for greater test coverage (i.e. rather than those types being the
  // most appropriate choice.)
  label_accessibility()->OverrideName("Label's Name",
                                      ax::mojom::NameFrom::kContents);
  EXPECT_EQ(label_accessibility()->GetName(), "Label's Name");
  EXPECT_EQ(label_accessibility()->GetNameFrom(),
            ax::mojom::NameFrom::kContents);

  label_accessibility()->OverrideDescription(
      "Label's description", ax::mojom::DescriptionFrom::kTitle);
  EXPECT_EQ(label_accessibility()->GetDescription(), "Label's description");
  EXPECT_EQ(label_accessibility()->GetDescriptionFrom(),
            ax::mojom::DescriptionFrom::kTitle);

  // Set the label's View as the name source of the accessible button.
  // This should cause the previously-set name to be replaced with the
  // accessible name of the label.
  button_accessibility()->OverrideLabelledBy(label_);
  EXPECT_EQ(button_accessibility()->GetName(), "Label's Name");
  EXPECT_EQ(button_accessibility()->GetNameFrom(),
            ax::mojom::NameFrom::kRelatedElement);

  // Setting the labelledby View to itself should trigger a DCHECK.
  EXPECT_DCHECK_DEATH_WITH(button_accessibility()->OverrideLabelledBy(button_),
                           "Check failed: labelled_by_view != view_");
}

TEST_F(ViewAXPlatformNodeDelegateTest, IsOrderedSet) {
  View::Views group_ids = SetUpExtraViews();
  SetUpExtraViewsGroups(group_ids);
  // Only last element has no group id.
  EXPECT_TRUE(view_accessibility(group_ids[0])->IsOrderedSet());
  EXPECT_TRUE(view_accessibility(group_ids[1])->IsOrderedSet());
  EXPECT_TRUE(view_accessibility(group_ids[2])->IsOrderedSet());
  EXPECT_TRUE(view_accessibility(group_ids[3])->IsOrderedSet());
  EXPECT_FALSE(view_accessibility(group_ids[4])->IsOrderedSet());

  EXPECT_TRUE(view_accessibility(group_ids[0])->IsOrderedSetItem());
  EXPECT_TRUE(view_accessibility(group_ids[1])->IsOrderedSetItem());
  EXPECT_TRUE(view_accessibility(group_ids[2])->IsOrderedSetItem());
  EXPECT_TRUE(view_accessibility(group_ids[3])->IsOrderedSetItem());
  EXPECT_FALSE(view_accessibility(group_ids[4])->IsOrderedSetItem());

  View::Views overrides = SetUpExtraViews();
  SetUpExtraViewsSetOverrides(overrides);
  // Only overrides[3] has no override values for setSize/ posInSet.
  EXPECT_TRUE(view_accessibility(overrides[0])->IsOrderedSet());
  EXPECT_TRUE(view_accessibility(overrides[1])->IsOrderedSet());
  EXPECT_TRUE(view_accessibility(overrides[2])->IsOrderedSet());
  EXPECT_FALSE(view_accessibility(overrides[3])->IsOrderedSet());
  EXPECT_TRUE(view_accessibility(overrides[4])->IsOrderedSet());

  EXPECT_TRUE(view_accessibility(overrides[0])->IsOrderedSetItem());
  EXPECT_TRUE(view_accessibility(overrides[1])->IsOrderedSetItem());
  EXPECT_TRUE(view_accessibility(overrides[2])->IsOrderedSetItem());
  EXPECT_FALSE(view_accessibility(overrides[3])->IsOrderedSetItem());
  EXPECT_TRUE(view_accessibility(overrides[4])->IsOrderedSetItem());
}

TEST_F(ViewAXPlatformNodeDelegateTest, SetSizeAndPosition) {
  // Test Views with group ids.
  View::Views group_ids = SetUpExtraViews();
  SetUpExtraViewsGroups(group_ids);
  EXPECT_EQ(view_accessibility(group_ids[0])->GetSetSize(), 3);
  EXPECT_EQ(view_accessibility(group_ids[0])->GetPosInSet(), 1);
  EXPECT_EQ(view_accessibility(group_ids[1])->GetSetSize(), 3);
  EXPECT_EQ(view_accessibility(group_ids[1])->GetPosInSet(), 2);
  EXPECT_EQ(view_accessibility(group_ids[2])->GetSetSize(), 3);
  EXPECT_EQ(view_accessibility(group_ids[2])->GetPosInSet(), 3);

  EXPECT_EQ(view_accessibility(group_ids[3])->GetSetSize(), 1);
  EXPECT_EQ(view_accessibility(group_ids[3])->GetPosInSet(), 1);

  EXPECT_FALSE(view_accessibility(group_ids[4])->GetSetSize().has_value());
  EXPECT_FALSE(view_accessibility(group_ids[4])->GetPosInSet().has_value());

  // Check if a View is ignored, it is not counted in SetSize or PosInSet
  group_ids[1]->GetViewAccessibility().OverrideIsIgnored(true);
  group_ids[2]->GetViewAccessibility().OverrideIsIgnored(true);
  EXPECT_EQ(view_accessibility(group_ids[0])->GetSetSize(), 1);
  EXPECT_EQ(view_accessibility(group_ids[0])->GetPosInSet(), 1);
  EXPECT_FALSE(view_accessibility(group_ids[1])->GetSetSize().has_value());
  EXPECT_FALSE(view_accessibility(group_ids[1])->GetPosInSet().has_value());
  EXPECT_FALSE(view_accessibility(group_ids[2])->GetSetSize().has_value());
  EXPECT_FALSE(view_accessibility(group_ids[2])->GetPosInSet().has_value());
  group_ids[1]->GetViewAccessibility().OverrideIsIgnored(false);
  group_ids[2]->GetViewAccessibility().OverrideIsIgnored(false);

  // Test Views with setSize/ posInSet override values set.
  View::Views overrides = SetUpExtraViews();
  SetUpExtraViewsSetOverrides(overrides);
  EXPECT_EQ(view_accessibility(overrides[0])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(overrides[0])->GetPosInSet(), 4);
  EXPECT_EQ(view_accessibility(overrides[1])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(overrides[1])->GetPosInSet(), 3);
  EXPECT_EQ(view_accessibility(overrides[2])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(overrides[2])->GetPosInSet(), 2);

  EXPECT_FALSE(view_accessibility(overrides[3])->GetSetSize().has_value());
  EXPECT_FALSE(view_accessibility(overrides[3])->GetPosInSet().has_value());

  EXPECT_EQ(view_accessibility(overrides[4])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(overrides[4])->GetPosInSet(), 1);

  // Test Views with both group ids and setSize/ posInSet override values set.
  // Make sure the override values take precedence when both are set.
  // Add setSize/ posInSet overrides to the Views with group ids.
  SetUpExtraViewsSetOverrides(group_ids);
  EXPECT_EQ(view_accessibility(group_ids[0])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(group_ids[0])->GetPosInSet(), 4);
  EXPECT_EQ(view_accessibility(group_ids[1])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(group_ids[1])->GetPosInSet(), 3);
  EXPECT_EQ(view_accessibility(group_ids[2])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(group_ids[2])->GetPosInSet(), 2);

  EXPECT_EQ(view_accessibility(group_ids[3])->GetSetSize(), 1);
  EXPECT_EQ(view_accessibility(group_ids[3])->GetPosInSet(), 1);

  EXPECT_EQ(view_accessibility(group_ids[4])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(group_ids[4])->GetPosInSet(), 1);
}

TEST_F(ViewAXPlatformNodeDelegateTest, TreeNavigation) {
  // Adds one extra parent view with four child views to our widget. The parent
  // view is added as the next sibling of the already present button view.
  //
  // Widget
  // ++NonClientView
  // ++NonClientFrameView
  // ++Button
  // ++++Label
  // 0 = ++ParentView
  // 1 = ++++ChildView1
  // 2 = ++++ChildView2
  // 3 = ++++ChildView3
  // 4 = ++++ChildView4
  View::Views extra_views = SetUpExtraViews();
  ViewAXPlatformNodeDelegate* parent_view = view_accessibility(extra_views[0]);
  ViewAXPlatformNodeDelegate* child_view_1 = view_accessibility(extra_views[1]);
  ViewAXPlatformNodeDelegate* child_view_2 = view_accessibility(extra_views[2]);
  ViewAXPlatformNodeDelegate* child_view_3 = view_accessibility(extra_views[3]);
  ViewAXPlatformNodeDelegate* child_view_4 = view_accessibility(extra_views[4]);

  EXPECT_EQ(view_accessibility(widget_->GetRootView())->GetNativeObject(),
            parent_view->GetParent());
  EXPECT_EQ(4u, parent_view->GetChildCount());

  EXPECT_EQ(0u, button_accessibility()->GetIndexInParent());
  EXPECT_EQ(1u, parent_view->GetIndexInParent());

  EXPECT_EQ(child_view_1->GetNativeObject(), parent_view->ChildAtIndex(0));
  EXPECT_EQ(child_view_2->GetNativeObject(), parent_view->ChildAtIndex(1));
  EXPECT_EQ(child_view_3->GetNativeObject(), parent_view->ChildAtIndex(2));
  EXPECT_EQ(child_view_4->GetNativeObject(), parent_view->ChildAtIndex(3));

  EXPECT_EQ(nullptr, parent_view->GetNextSibling());
  EXPECT_EQ(button_accessibility()->GetNativeObject(),
            parent_view->GetPreviousSibling());

  EXPECT_EQ(parent_view->GetNativeObject(), child_view_1->GetParent());
  EXPECT_EQ(0u, child_view_1->GetChildCount());
  EXPECT_EQ(0u, child_view_1->GetIndexInParent());
  EXPECT_EQ(child_view_2->GetNativeObject(), child_view_1->GetNextSibling());
  EXPECT_EQ(nullptr, child_view_1->GetPreviousSibling());

  EXPECT_EQ(parent_view->GetNativeObject(), child_view_2->GetParent());
  EXPECT_EQ(0u, child_view_2->GetChildCount());
  EXPECT_EQ(1u, child_view_2->GetIndexInParent());
  EXPECT_EQ(child_view_3->GetNativeObject(), child_view_2->GetNextSibling());
  EXPECT_EQ(child_view_1->GetNativeObject(),
            child_view_2->GetPreviousSibling());

  EXPECT_EQ(parent_view->GetNativeObject(), child_view_3->GetParent());
  EXPECT_EQ(0u, child_view_3->GetChildCount());
  EXPECT_EQ(2u, child_view_3->GetIndexInParent());
  EXPECT_EQ(child_view_4->GetNativeObject(), child_view_3->GetNextSibling());
  EXPECT_EQ(child_view_2->GetNativeObject(),
            child_view_3->GetPreviousSibling());

  EXPECT_EQ(parent_view->GetNativeObject(), child_view_4->GetParent());
  EXPECT_EQ(0u, child_view_4->GetChildCount());
  EXPECT_EQ(3u, child_view_4->GetIndexInParent());
  EXPECT_EQ(nullptr, child_view_4->GetNextSibling());
  EXPECT_EQ(child_view_3->GetNativeObject(),
            child_view_4->GetPreviousSibling());
}

TEST_F(ViewAXPlatformNodeDelegateTest, TreeNavigationWithLeafViews) {
  // Adds one extra parent view with four child views to our widget. The parent
  // view is added as the next sibling of the already present button view.
  //
  // Widget
  // ++Button
  // ++++Label
  // 0 = ++ParentView
  // 1 = ++++ChildView1
  // 2 = ++++ChildView2
  // 3 = ++++ChildView3
  // 4 = ++++ChildView4
  View::Views extra_views = SetUpExtraViews();
  ViewAXPlatformNodeDelegate* contents_view =
      view_accessibility(widget_->GetRootView());
  ViewAXPlatformNodeDelegate* parent_view = view_accessibility(extra_views[0]);
  ViewAXPlatformNodeDelegate* child_view_1 = view_accessibility(extra_views[1]);
  ViewAXPlatformNodeDelegate* child_view_2 = view_accessibility(extra_views[2]);
  ViewAXPlatformNodeDelegate* child_view_3 = view_accessibility(extra_views[3]);
  ViewAXPlatformNodeDelegate* child_view_4 = view_accessibility(extra_views[4]);

  // Mark the parent view and the second child view as leafs. This should hide
  // all four children, not only the second child. It should not hide the parent
  // view. In this context, "hide" means that these views will be ignored (be
  // invisible) by platform accessibility APIs.
  parent_view->OverrideIsLeaf(true);
  child_view_2->OverrideIsLeaf(true);

  EXPECT_EQ(2u, contents_view->GetChildCount());
  EXPECT_EQ(contents_view->GetNativeObject(), parent_view->GetParent());
  EXPECT_EQ(0u, parent_view->GetChildCount());

  EXPECT_EQ(0u, button_accessibility()->GetIndexInParent());
  EXPECT_EQ(1u, parent_view->GetIndexInParent());

  EXPECT_FALSE(contents_view->IsIgnored());
  EXPECT_FALSE(parent_view->IsIgnored());
  EXPECT_TRUE(child_view_1->IsIgnored());
  EXPECT_TRUE(child_view_2->IsIgnored());
  EXPECT_TRUE(child_view_3->IsIgnored());
  EXPECT_TRUE(child_view_4->IsIgnored());

  EXPECT_FALSE(contents_view->IsLeaf());
  EXPECT_TRUE(parent_view->IsLeaf());

  EXPECT_FALSE(contents_view->IsChildOfLeaf());
  EXPECT_FALSE(parent_view->IsChildOfLeaf());
#if !BUILDFLAG(USE_ATK)
  // TODO(crbug.com/1100047): IsChildOfLeaf always returns false on Linux.
  EXPECT_TRUE(child_view_1->IsChildOfLeaf());
  EXPECT_TRUE(child_view_2->IsChildOfLeaf());
  EXPECT_TRUE(child_view_3->IsChildOfLeaf());
  EXPECT_TRUE(child_view_4->IsChildOfLeaf());
#endif  // !BUILDFLAG(USE_ATK)

  EXPECT_EQ(parent_view->GetNativeObject(), child_view_1->GetParent());
  EXPECT_EQ(parent_view->GetNativeObject(), child_view_2->GetParent());
  EXPECT_EQ(parent_view->GetNativeObject(), child_view_3->GetParent());
  EXPECT_EQ(parent_view->GetNativeObject(), child_view_4->GetParent());

  // Try unhiding the parent view's descendants. Nothing should be hidden any
  // more. The second child has no descendants so marking it as a leaf should
  // have no effect.
  parent_view->OverrideIsLeaf(false);

  EXPECT_EQ(2u, contents_view->GetChildCount());
  EXPECT_EQ(contents_view->GetNativeObject(), parent_view->GetParent());
  EXPECT_EQ(4u, parent_view->GetChildCount());

  EXPECT_EQ(0u, button_accessibility()->GetIndexInParent());
  EXPECT_EQ(1u, parent_view->GetIndexInParent());

  EXPECT_FALSE(contents_view->IsIgnored());
  EXPECT_FALSE(parent_view->IsIgnored());
  EXPECT_FALSE(child_view_1->IsIgnored());
  EXPECT_FALSE(child_view_2->IsIgnored());
  EXPECT_FALSE(child_view_3->IsIgnored());
  EXPECT_FALSE(child_view_4->IsIgnored());

  EXPECT_FALSE(contents_view->IsLeaf());
  EXPECT_FALSE(parent_view->IsLeaf());
  EXPECT_TRUE(child_view_1->IsLeaf());
  EXPECT_TRUE(child_view_2->IsLeaf());
  EXPECT_TRUE(child_view_3->IsLeaf());
  EXPECT_TRUE(child_view_4->IsLeaf());

  EXPECT_FALSE(contents_view->IsChildOfLeaf());
  EXPECT_FALSE(parent_view->IsChildOfLeaf());
  EXPECT_FALSE(child_view_1->IsChildOfLeaf());
  EXPECT_FALSE(child_view_2->IsChildOfLeaf());
  EXPECT_FALSE(child_view_3->IsChildOfLeaf());
  EXPECT_FALSE(child_view_4->IsChildOfLeaf());

  EXPECT_EQ(parent_view->GetNativeObject(), child_view_1->GetParent());
  EXPECT_EQ(parent_view->GetNativeObject(), child_view_2->GetParent());
  EXPECT_EQ(parent_view->GetNativeObject(), child_view_3->GetParent());
  EXPECT_EQ(parent_view->GetNativeObject(), child_view_4->GetParent());

  EXPECT_EQ(child_view_1->GetNativeObject(), parent_view->ChildAtIndex(0));
  EXPECT_EQ(child_view_2->GetNativeObject(), parent_view->ChildAtIndex(1));
  EXPECT_EQ(child_view_3->GetNativeObject(), parent_view->ChildAtIndex(2));
  EXPECT_EQ(child_view_4->GetNativeObject(), parent_view->ChildAtIndex(3));
}

TEST_F(ViewAXPlatformNodeDelegateTest, TreeNavigationWithIgnoredViews) {
  // Adds one extra parent view with four child views to our widget. The parent
  // view is added as the next sibling of the already present button view.
  //
  // Widget
  // ++Button
  // ++++Label
  // 0 = ++ParentView
  // 1 = ++++ChildView1
  // 2 = ++++ChildView2
  // 3 = ++++ChildView3
  // 4 = ++++ChildView4
  View::Views extra_views = SetUpExtraViews();
  ViewAXPlatformNodeDelegate* contents_view =
      view_accessibility(widget_->GetRootView());
  ViewAXPlatformNodeDelegate* parent_view = view_accessibility(extra_views[0]);
  ViewAXPlatformNodeDelegate* child_view_1 = view_accessibility(extra_views[1]);
  ViewAXPlatformNodeDelegate* child_view_2 = view_accessibility(extra_views[2]);
  ViewAXPlatformNodeDelegate* child_view_3 = view_accessibility(extra_views[3]);
  ViewAXPlatformNodeDelegate* child_view_4 = view_accessibility(extra_views[4]);

  // Mark the parent view and the second child view as ignored.
  parent_view->OverrideIsIgnored(true);
  child_view_2->OverrideIsIgnored(true);

  EXPECT_EQ(contents_view->GetNativeObject(), parent_view->GetParent());
  EXPECT_EQ(3u, parent_view->GetChildCount());

  EXPECT_EQ(0u, button_accessibility()->GetIndexInParent());
  EXPECT_FALSE(parent_view->GetIndexInParent().has_value());

  EXPECT_EQ(child_view_1->GetNativeObject(), parent_view->ChildAtIndex(0));
  EXPECT_EQ(child_view_3->GetNativeObject(), parent_view->ChildAtIndex(1));
  EXPECT_EQ(child_view_4->GetNativeObject(), parent_view->ChildAtIndex(2));

  EXPECT_EQ(button_accessibility()->GetNativeObject(),
            contents_view->ChildAtIndex(0));
  EXPECT_EQ(child_view_1->GetNativeObject(), contents_view->ChildAtIndex(1));
  EXPECT_EQ(child_view_3->GetNativeObject(), contents_view->ChildAtIndex(2));
  EXPECT_EQ(child_view_4->GetNativeObject(), contents_view->ChildAtIndex(3));

  EXPECT_EQ(nullptr, parent_view->GetNextSibling());
  EXPECT_EQ(nullptr, parent_view->GetPreviousSibling());

  EXPECT_EQ(contents_view->GetNativeObject(), child_view_1->GetParent());
  EXPECT_EQ(0u, child_view_1->GetChildCount());
  EXPECT_EQ(1u, child_view_1->GetIndexInParent());
  EXPECT_EQ(child_view_3->GetNativeObject(), child_view_1->GetNextSibling());
  EXPECT_EQ(button_accessibility()->GetNativeObject(),
            child_view_1->GetPreviousSibling());

  EXPECT_EQ(contents_view->GetNativeObject(), child_view_2->GetParent());
  EXPECT_EQ(0u, child_view_2->GetChildCount());
  EXPECT_FALSE(child_view_2->GetIndexInParent().has_value());
  EXPECT_EQ(nullptr, child_view_2->GetNextSibling());
  EXPECT_EQ(nullptr, child_view_2->GetPreviousSibling());

  EXPECT_EQ(contents_view->GetNativeObject(), child_view_3->GetParent());
  EXPECT_EQ(0u, child_view_3->GetChildCount());
  EXPECT_EQ(2u, child_view_3->GetIndexInParent());
  EXPECT_EQ(child_view_4->GetNativeObject(), child_view_3->GetNextSibling());
  EXPECT_EQ(child_view_1->GetNativeObject(),
            child_view_3->GetPreviousSibling());

  EXPECT_EQ(contents_view->GetNativeObject(), child_view_4->GetParent());
  EXPECT_EQ(0u, child_view_4->GetChildCount());
  EXPECT_EQ(3u, child_view_4->GetIndexInParent());
  EXPECT_EQ(nullptr, child_view_4->GetNextSibling());
  EXPECT_EQ(child_view_3->GetNativeObject(),
            child_view_4->GetPreviousSibling());
}

TEST_F(ViewAXPlatformNodeDelegateTest, OverrideIsEnabled) {
  // Initially, the button should be enabled.
  EXPECT_TRUE(button_accessibility()->IsAccessibilityEnabled());
  EXPECT_TRUE(button_accessibility()->IsAccessibilityFocusable());

  button_->SetEnabled(false);
  EXPECT_FALSE(button_accessibility()->IsAccessibilityEnabled());
  EXPECT_FALSE(button_accessibility()->IsAccessibilityFocusable());

  button_->SetEnabled(true);
  EXPECT_TRUE(button_accessibility()->IsAccessibilityEnabled());
  EXPECT_TRUE(button_accessibility()->IsAccessibilityFocusable());

  // `ViewAccessibility::OverrideIsEnabled` should have priority over
  // `View::SetEnabled`.
  button_accessibility()->OverrideIsEnabled(false);
  EXPECT_FALSE(button_accessibility()->IsAccessibilityEnabled());
  EXPECT_FALSE(button_accessibility()->IsAccessibilityFocusable());

  button_->SetEnabled(false);
  EXPECT_FALSE(button_accessibility()->IsAccessibilityEnabled());
  EXPECT_FALSE(button_accessibility()->IsAccessibilityFocusable());
  button_accessibility()->OverrideIsEnabled(true);
  EXPECT_TRUE(button_accessibility()->IsAccessibilityEnabled());
  EXPECT_TRUE(button_accessibility()->IsAccessibilityFocusable());

  // Initially, the label should be enabled. It should never be focusable
  // because it is not an interactive control like the button.
  EXPECT_TRUE(label_accessibility()->IsAccessibilityEnabled());
  EXPECT_FALSE(label_accessibility()->IsAccessibilityFocusable());

  label_->SetEnabled(false);
  EXPECT_FALSE(label_accessibility()->IsAccessibilityEnabled());
  EXPECT_FALSE(label_accessibility()->IsAccessibilityFocusable());

  label_accessibility()->OverrideIsEnabled(true);
  EXPECT_TRUE(label_accessibility()->IsAccessibilityEnabled());
  EXPECT_FALSE(label_accessibility()->IsAccessibilityFocusable());

  label_accessibility()->OverrideIsEnabled(false);
  EXPECT_FALSE(label_accessibility()->IsAccessibilityEnabled());
  EXPECT_FALSE(label_accessibility()->IsAccessibilityFocusable());
}

TEST_F(ViewAXPlatformNodeDelegateTest, OverrideHasPopup) {
  View::Views view_ids = SetUpExtraViews();

  view_ids[1]->GetViewAccessibility().OverrideHasPopup(
      ax::mojom::HasPopup::kTrue);
  view_ids[2]->GetViewAccessibility().OverrideHasPopup(
      ax::mojom::HasPopup::kMenu);

  ui::AXNodeData node_data_0;
  view_ids[0]->GetViewAccessibility().GetAccessibleNodeData(&node_data_0);
  EXPECT_EQ(node_data_0.GetHasPopup(), ax::mojom::HasPopup::kFalse);

  ui::AXNodeData node_data_1;
  view_ids[1]->GetViewAccessibility().GetAccessibleNodeData(&node_data_1);
  EXPECT_EQ(node_data_1.GetHasPopup(), ax::mojom::HasPopup::kTrue);

  ui::AXNodeData node_data_2;
  view_ids[2]->GetViewAccessibility().GetAccessibleNodeData(&node_data_2);
  EXPECT_EQ(node_data_2.GetHasPopup(), ax::mojom::HasPopup::kMenu);
}

TEST_F(ViewAXPlatformNodeDelegateTest, FocusOnMenuClose) {
  // Set Focus on the button
  button_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  EXPECT_EQ(nullptr, button_->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(nullptr, button_accessibility()->GetFocus());

  EXPECT_TRUE(SetFocused(button_accessibility(), true));
  EXPECT_EQ(button_->GetNativeViewAccessible(),
            button_accessibility()->GetFocus());

  // Fire FocusAfterMenuClose event on the button.
  base::RunLoop run_loop;
  ui::AXPlatformNodeBase::SetOnNotifyEventCallbackForTesting(
      ax::mojom::Event::kFocusAfterMenuClose, run_loop.QuitClosure());
  button_accessibility()->FireFocusAfterMenuClose();
  run_loop.Run();
  EXPECT_EQ(button_->GetNativeViewAccessible(),
            button_accessibility()->GetFocus());
}

TEST_F(ViewAXPlatformNodeDelegateTableTest, TableHasHeader) {
  EXPECT_TRUE(table_accessibility()->TableHasColumnOrRowHeaderNodeForTesting());
  EXPECT_EQ(size_t{4}, table_accessibility()->GetColHeaderNodeIds().size());
  EXPECT_TRUE(table_accessibility()->GetColHeaderNodeIds(5).empty());
}

TEST_F(ViewAXPlatformNodeDelegateTableTest, TableHasCell) {
  EXPECT_NE(absl::nullopt, table_accessibility()->GetCellId(0, 0));
  EXPECT_NE(absl::nullopt, table_accessibility()->GetCellId(0, 3));
  EXPECT_NE(absl::nullopt, table_accessibility()->GetCellId(9, 3));
  EXPECT_DCHECK_DEATH(table_accessibility()->GetCellId(-1, 0));
  EXPECT_DCHECK_DEATH(table_accessibility()->GetCellId(0, -1));
  EXPECT_DCHECK_DEATH(table_accessibility()->GetCellId(10, 0));
  EXPECT_DCHECK_DEATH(table_accessibility()->GetCellId(0, 4));
}

TEST_F(ViewAXPlatformNodeDelegateMenuTest, MenuTest) {
  RunMenu();

  ViewAXPlatformNodeDelegate* submenu = submenu_accessibility();
  EXPECT_FALSE(submenu->HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(submenu->GetChildCount(), 8u);
  EXPECT_EQ(submenu->GetRole(), ax::mojom::Role::kMenu);
  EXPECT_EQ(submenu->GetData().GetHasPopup(), ax::mojom::HasPopup::kMenu);

  auto items = submenu->view()->children();

  // MenuItemView::Type::kNormal
  ViewAXPlatformNodeDelegate* normal_item = view_accessibility(items[0]);
  EXPECT_TRUE(normal_item->HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(normal_item->GetData().IsSelectable());
  EXPECT_FALSE(
      normal_item->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(normal_item->IsInvisibleOrIgnored());
  EXPECT_FALSE(normal_item->GetData().IsInvisibleOrIgnored());
  EXPECT_EQ(normal_item->GetRole(), ax::mojom::Role::kMenuItem);
  EXPECT_EQ(normal_item->GetData().GetHasPopup(), ax::mojom::HasPopup::kFalse);
  EXPECT_EQ(normal_item->GetPosInSet(), 1);
  EXPECT_EQ(normal_item->GetSetSize(), 7);
  EXPECT_EQ(normal_item->GetChildCount(), 0u);
  EXPECT_EQ(normal_item->GetIndexInParent(), 0u);

  // MenuItemView::Type::kSubMenu
  ViewAXPlatformNodeDelegate* submenu_item = view_accessibility(items[1]);
  EXPECT_TRUE(submenu_item->HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(submenu_item->GetData().IsSelectable());
  EXPECT_FALSE(
      submenu_item->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(submenu_item->IsInvisibleOrIgnored());
  EXPECT_FALSE(submenu_item->GetData().IsInvisibleOrIgnored());
  EXPECT_EQ(submenu_item->GetRole(), ax::mojom::Role::kMenuItem);
  EXPECT_EQ(submenu_item->GetData().GetHasPopup(), ax::mojom::HasPopup::kMenu);
  EXPECT_EQ(submenu_item->GetPosInSet(), 2);
  EXPECT_EQ(submenu_item->GetSetSize(), 7);
#if BUILDFLAG(IS_MAC)
  // A virtual child with role menu is exposed so that VoiceOver treats a
  // MenuItemView of type kSubMenu as a submenu rather than an item.
  EXPECT_EQ(submenu_item->GetChildCount(), 1u);
#else
  EXPECT_EQ(submenu_item->GetChildCount(), 0u);
#endif  // BUILDFLAG(IS_MAC)
  EXPECT_EQ(submenu_item->GetIndexInParent(), 1u);

  // MenuItemView::Type::kActionableSubMenu
  ViewAXPlatformNodeDelegate* actionable_submenu_item =
      view_accessibility(items[2]);
  EXPECT_TRUE(actionable_submenu_item->HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(actionable_submenu_item->GetData().IsSelectable());
  EXPECT_FALSE(actionable_submenu_item->GetBoolAttribute(
      ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(actionable_submenu_item->IsInvisibleOrIgnored());
  EXPECT_FALSE(actionable_submenu_item->GetData().IsInvisibleOrIgnored());
  EXPECT_EQ(actionable_submenu_item->GetRole(), ax::mojom::Role::kMenuItem);
  EXPECT_EQ(actionable_submenu_item->GetData().GetHasPopup(),
            ax::mojom::HasPopup::kMenu);
  EXPECT_EQ(actionable_submenu_item->GetPosInSet(), 3);
  EXPECT_EQ(actionable_submenu_item->GetSetSize(), 7);
#if BUILDFLAG(IS_MAC)
  // A virtual child with role menu is exposed so that VoiceOver treats a
  // MenuItemView of type kActionableSubMenu as a submenu rather than an item.
  EXPECT_EQ(actionable_submenu_item->GetChildCount(), 1u);
#else
  EXPECT_EQ(actionable_submenu_item->GetChildCount(), 0u);
#endif  // BUILDFLAG(IS_MAC)
  EXPECT_EQ(actionable_submenu_item->GetIndexInParent(), 2u);

  // MenuItemView::Type::kCheckbox
  ViewAXPlatformNodeDelegate* checkbox_item = view_accessibility(items[3]);
  EXPECT_TRUE(checkbox_item->HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(checkbox_item->GetData().IsSelectable());
  EXPECT_TRUE(
      checkbox_item->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(checkbox_item->IsInvisibleOrIgnored());
  EXPECT_FALSE(checkbox_item->GetData().IsInvisibleOrIgnored());
  EXPECT_EQ(checkbox_item->GetRole(), ax::mojom::Role::kMenuItemCheckBox);
  EXPECT_EQ(checkbox_item->GetData().GetHasPopup(),
            ax::mojom::HasPopup::kFalse);
  EXPECT_EQ(checkbox_item->GetPosInSet(), 4);
  EXPECT_EQ(checkbox_item->GetSetSize(), 7);
  EXPECT_EQ(checkbox_item->GetChildCount(), 0u);
  EXPECT_EQ(checkbox_item->GetIndexInParent(), 3u);

  // MenuItemView::Type::kRadio
  ViewAXPlatformNodeDelegate* radio_item = view_accessibility(items[4]);
  EXPECT_TRUE(radio_item->HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(radio_item->GetData().IsSelectable());
  EXPECT_FALSE(
      radio_item->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(radio_item->IsInvisibleOrIgnored());
  EXPECT_FALSE(radio_item->GetData().IsInvisibleOrIgnored());
  EXPECT_EQ(radio_item->GetRole(), ax::mojom::Role::kMenuItemRadio);
  EXPECT_EQ(radio_item->GetData().GetHasPopup(), ax::mojom::HasPopup::kFalse);
  EXPECT_EQ(radio_item->GetPosInSet(), 5);
  EXPECT_EQ(radio_item->GetSetSize(), 7);
  EXPECT_EQ(radio_item->GetChildCount(), 0u);
  EXPECT_EQ(radio_item->GetIndexInParent(), 4u);

  // MenuItemView::Type::kSeparator
  ViewAXPlatformNodeDelegate* separator_item = view_accessibility(items[5]);
  EXPECT_FALSE(separator_item->HasState(ax::mojom::State::kFocusable));
  EXPECT_FALSE(separator_item->GetData().IsSelectable());
  EXPECT_FALSE(
      separator_item->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(separator_item->IsInvisibleOrIgnored());
  EXPECT_FALSE(separator_item->GetData().IsInvisibleOrIgnored());
  EXPECT_EQ(separator_item->GetRole(), ax::mojom::Role::kSplitter);
  EXPECT_EQ(separator_item->GetData().GetHasPopup(),
            ax::mojom::HasPopup::kFalse);
  EXPECT_FALSE(
      separator_item->HasIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_FALSE(
      separator_item->HasIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(separator_item->GetChildCount(), 0u);
  EXPECT_EQ(separator_item->GetIndexInParent(), 5u);

  // MenuItemView::Type::kHighlighted
  ViewAXPlatformNodeDelegate* highlighted_item = view_accessibility(items[6]);
  EXPECT_TRUE(highlighted_item->HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(highlighted_item->GetData().IsSelectable());
  EXPECT_FALSE(
      highlighted_item->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(highlighted_item->IsInvisibleOrIgnored());
  EXPECT_FALSE(highlighted_item->GetData().IsInvisibleOrIgnored());
  EXPECT_EQ(highlighted_item->GetRole(), ax::mojom::Role::kMenuItem);
  EXPECT_EQ(highlighted_item->GetData().GetHasPopup(),
            ax::mojom::HasPopup::kFalse);
  EXPECT_EQ(highlighted_item->GetPosInSet(), 6);
  EXPECT_EQ(highlighted_item->GetSetSize(), 7);
  EXPECT_EQ(highlighted_item->GetChildCount(), 0u);
  EXPECT_EQ(highlighted_item->GetIndexInParent(), 6u);

  // MenuItemView::Type::kTitle
  ViewAXPlatformNodeDelegate* title_item = view_accessibility(items[7]);
  EXPECT_TRUE(title_item->HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(title_item->GetData().IsSelectable());
  EXPECT_FALSE(
      title_item->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(title_item->IsInvisibleOrIgnored());
  EXPECT_FALSE(title_item->GetData().IsInvisibleOrIgnored());
  EXPECT_EQ(title_item->GetRole(), ax::mojom::Role::kMenuItem);
  EXPECT_EQ(title_item->GetData().GetHasPopup(), ax::mojom::HasPopup::kFalse);
  EXPECT_EQ(title_item->GetPosInSet(), 7);
  EXPECT_EQ(title_item->GetSetSize(), 7);
  EXPECT_EQ(title_item->GetChildCount(), 0u);
  EXPECT_EQ(title_item->GetIndexInParent(), 7u);
}

#if defined(USE_AURA)
class DerivedTestView : public View {
 public:
  DerivedTestView() = default;
  ~DerivedTestView() override = default;

  void OnBlur() override { SetVisible(false); }
};

using AXViewTest = ViewsTestBase;

// Check if the destruction of the widget ends successfully if |view|'s
// visibility changed during destruction.
TEST_F(AXViewTest, LayoutCalledInvalidateRootView) {
  // TODO(jamescook): Construct a real AutomationManagerAura rather than using
  // this observer to simulate it.
  AXAuraObjCache cache;
  TestAXEventObserver observer(&cache);
  UniqueWidgetPtr widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(params));
  widget->Show();

  View* root = widget->GetRootView();
  DerivedTestView* parent = new DerivedTestView();
  DerivedTestView* child = new DerivedTestView();
  root->AddChildView(parent);
  parent->AddChildView(child);
  child->SetFocusBehavior(DerivedTestView::FocusBehavior::ALWAYS);
  parent->SetFocusBehavior(DerivedTestView::FocusBehavior::ALWAYS);
  root->SetFocusBehavior(DerivedTestView::FocusBehavior::ALWAYS);
  parent->RequestFocus();
  // During the destruction of parent, OnBlur will be called and change the
  // visibility to false.
  parent->SetVisible(true);

  cache.GetOrCreate(widget.get());
}
#endif

}  // namespace views::test
