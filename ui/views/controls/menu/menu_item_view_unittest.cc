// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_item_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/menu/test_menu_item_view.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_test_api.h"
#include "ui/views/view_utils.h"

namespace views {

using MenuItemViewUnitTest = ViewsTestBase;

TEST_F(MenuItemViewUnitTest, AddAndRemoveChildren) {
  views::TestMenuItemView root_menu;

  auto* item = root_menu.AppendMenuItem(0);

  views::SubmenuView* submenu = root_menu.GetSubmenu();
  ASSERT_TRUE(submenu);
  const auto menu_items = submenu->GetMenuItems();
  ASSERT_EQ(1u, menu_items.size());
  EXPECT_EQ(item, menu_items.front());

  root_menu.RemoveMenuItem(item);

  EXPECT_TRUE(submenu->GetMenuItems().empty());
}

namespace {

// A simple View class that will match its height to the available width.
class SquareView : public views::View {
  METADATA_HEADER(SquareView, views::View)

 public:
  SquareView() = default;
  ~SquareView() override = default;

 private:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override {
    int width = available_size.width().value_or(1);
    return gfx::Size(width, width);
  }
};

BEGIN_METADATA(SquareView)
END_METADATA

}  // namespace

TEST_F(MenuItemViewUnitTest, TestMenuItemViewWithFlexibleWidthChild) {
  views::TestMenuItemView root_menu;

  // Append a normal MenuItemView.
  views::MenuItemView* label_view = root_menu.AppendMenuItem(1, u"item 1");

  // Append a second MenuItemView that has a child SquareView.
  views::MenuItemView* flexible_view = root_menu.AppendMenuItem(2);
  flexible_view->AddChildView(new SquareView());
  // Set margins to 0 so that we know width should match height.
  flexible_view->set_vertical_margin(0);

  views::SubmenuView* submenu = root_menu.GetSubmenu();

  // The first item should be the label view.
  ASSERT_EQ(label_view, submenu->GetMenuItemAt(0));
  gfx::Size label_size = label_view->GetPreferredSize({});

  // The second item should be the flexible view.
  ASSERT_EQ(flexible_view, submenu->GetMenuItemAt(1));
  gfx::Size flexible_size = flexible_view->GetPreferredSize({});

  EXPECT_EQ(1, flexible_size.width());

  // ...but it should use whatever space is available to make a square.
  int flex_height = flexible_view->GetHeightForWidth(label_size.width());
  EXPECT_EQ(label_size.width(), flex_height);

  // The submenu should be tall enough to allow for both menu items at the
  // given width. (It may be taller if there is padding between/around the
  // items.)
  EXPECT_GE(submenu->GetPreferredSize({}).height(),
            label_size.height() + flex_height);
}

// Tests that a menu item with hidden children should contain the "(empty)" menu
// item to display.
TEST_F(MenuItemViewUnitTest, TestEmptyWhenAllItemsAreHidden) {
  views::TestMenuItemView root_menu;
  views::MenuItemView* item1 = root_menu.AppendMenuItem(1, u"item 1");
  views::MenuItemView* item2 = root_menu.AppendMenuItem(2, u"item 2");

  // Set menu items to hidden.
  item1->SetVisible(false);
  item2->SetVisible(false);

  SubmenuView* submenu = root_menu.GetSubmenu();
  ASSERT_TRUE(submenu);

  EXPECT_EQ(2u, submenu->children().size());

  // Adds any empty menu items to the menu, if needed.
  root_menu.UpdateEmptyMenusAndMetrics();

  // Because all of the submenu's children are hidden, an empty menu item should
  // have been added.
  ASSERT_EQ(3u, submenu->children().size());
  const auto* empty_item =
      AsViewClass<EmptyMenuMenuItem>(submenu->children().front());
  ASSERT_TRUE(empty_item);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_MENU_EMPTY_SUBMENU),
            empty_item->title());
}

TEST_F(MenuItemViewUnitTest, UseMnemonicOnPlatform) {
  views::TestMenuItemView root_menu;
  views::MenuItemView* item1 = root_menu.AppendMenuItem(1, u"&Item 1");
  views::MenuItemView* item2 = root_menu.AppendMenuItem(2, u"I&tem 2");

  root_menu.set_has_mnemonics(true);

  if (MenuConfig::instance().use_mnemonics) {
    EXPECT_EQ('i', item1->GetMnemonic());
    EXPECT_EQ('t', item2->GetMnemonic());
  } else {
    EXPECT_EQ(0, item1->GetMnemonic());
    EXPECT_EQ(0, item2->GetMnemonic());
  }
}

TEST_F(MenuItemViewUnitTest, NotifiesSelectedChanged) {
  views::TestMenuItemView root_menu;

  // Append a MenuItemView.
  views::MenuItemView* menu_item_view = root_menu.AppendMenuItem(1, u"item");

  // Verify initial selected state.
  bool is_selected = menu_item_view->IsSelected();
  EXPECT_FALSE(is_selected);

  // Subscribe to be notified of changes to selected state.
  auto subscription =
      menu_item_view->AddSelectedChangedCallback(base::BindLambdaForTesting(
          [&]() { is_selected = menu_item_view->IsSelected(); }));

  // Verify we are notified when the MenuItemView becomes selected.
  menu_item_view->SetSelected(true);
  EXPECT_TRUE(is_selected);

  // Verify we are notified when the MenuItemView becomes deselected.
  menu_item_view->SetSelected(false);
  EXPECT_FALSE(is_selected);
}

TEST_F(MenuItemViewUnitTest, AccessibleKeyShortcutsTest) {
  views::TestMenuItemView root_menu;

  views::MenuItemView* item1 = root_menu.AppendMenuItem(1, u"&Item 1");
  views::MenuItemView* item2 = root_menu.AppendMenuItem(2, u"It&em 2");
  views::MenuItemView* item3 = root_menu.AppendSubMenu(1, u"Su&menu 1");
  SubmenuView* submenu = item3->GetSubmenu();

  ui::AXNodeData data1, data2, data3, data4;

  if (MenuConfig::instance().use_mnemonics) {
    item1->GetViewAccessibility().GetAccessibleNodeData(&data1);
    item2->GetViewAccessibility().GetAccessibleNodeData(&data2);
    item3->GetViewAccessibility().GetAccessibleNodeData(&data3);
    submenu->GetViewAccessibility().GetAccessibleNodeData(&data4);
    EXPECT_FALSE(
        data1.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_FALSE(
        data2.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_FALSE(
        data3.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_FALSE(
        data4.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));

    root_menu.set_has_mnemonics(true);
    data1 = ui::AXNodeData();
    data2 = ui::AXNodeData();
    data3 = ui::AXNodeData();
    data4 = ui::AXNodeData();
    item1->GetViewAccessibility().GetAccessibleNodeData(&data1);
    item2->GetViewAccessibility().GetAccessibleNodeData(&data2);
    item3->GetViewAccessibility().GetAccessibleNodeData(&data3);
    submenu->GetViewAccessibility().GetAccessibleNodeData(&data4);
    EXPECT_EQ("i", data1.GetStringAttribute(
                       ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_EQ("e", data2.GetStringAttribute(
                       ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_EQ("m", data3.GetStringAttribute(
                       ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_EQ("m", data4.GetStringAttribute(
                       ax::mojom::StringAttribute::kKeyShortcuts));

    item1->set_may_have_mnemonics(false);
    data1 = ui::AXNodeData();
    item1->GetViewAccessibility().GetAccessibleNodeData(&data1);
    EXPECT_FALSE(
        data1.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));

    root_menu.set_has_mnemonics(false);
    item1->set_may_have_mnemonics(true);
    data1 = ui::AXNodeData();
    data2 = ui::AXNodeData();
    data3 = ui::AXNodeData();
    data4 = ui::AXNodeData();
    item1->GetViewAccessibility().GetAccessibleNodeData(&data1);
    item2->GetViewAccessibility().GetAccessibleNodeData(&data2);
    item3->GetViewAccessibility().GetAccessibleNodeData(&data3);
    submenu->GetViewAccessibility().GetAccessibleNodeData(&data4);
    EXPECT_FALSE(
        data1.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_FALSE(
        data2.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_FALSE(
        data3.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_FALSE(
        data4.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
  } else {
    item1->GetViewAccessibility().GetAccessibleNodeData(&data1);
    item2->GetViewAccessibility().GetAccessibleNodeData(&data2);
    item3->GetViewAccessibility().GetAccessibleNodeData(&data3);
    submenu->GetViewAccessibility().GetAccessibleNodeData(&data4);
    EXPECT_FALSE(
        data1.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_FALSE(
        data2.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_FALSE(
        data3.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_FALSE(
        data4.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));

    root_menu.set_has_mnemonics(true);
    data1 = ui::AXNodeData();
    data2 = ui::AXNodeData();
    item1->GetViewAccessibility().GetAccessibleNodeData(&data1);
    item2->GetViewAccessibility().GetAccessibleNodeData(&data2);
    item3->GetViewAccessibility().GetAccessibleNodeData(&data3);
    submenu->GetViewAccessibility().GetAccessibleNodeData(&data4);
    EXPECT_FALSE(
        data1.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_FALSE(
        data2.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_FALSE(
        data3.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
    EXPECT_FALSE(
        data4.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts));
  }
}

TEST_F(MenuItemViewUnitTest, AccessibleProperties) {
  views::TestMenuItemView root_menu;
  views::MenuItemView* item1 = root_menu.AppendMenuItemImpl(
      0, u"checkbox", ui::ImageModel(), MenuItemView::Type::kCheckbox);
  views::MenuItemView* item2 = root_menu.AppendMenuItemImpl(
      1, u"radio", ui::ImageModel(), MenuItemView::Type::kRadio);
  views::MenuItemView* item3 = root_menu.AppendMenuItemImpl(
      2, u"title", ui::ImageModel(), MenuItemView::Type::kTitle);
  views::MenuItemView* item4 = root_menu.AppendMenuItemImpl(
      3, u"highlighted", ui::ImageModel(), MenuItemView::Type::kHighlighted);
  ui::AXNodeData data1, data2, data3, data4;

  item1->GetViewAccessibility().GetAccessibleNodeData(&data1);
  EXPECT_EQ(data1.role, ax::mojom::Role::kMenuItemCheckBox);

  item2->GetViewAccessibility().GetAccessibleNodeData(&data2);
  EXPECT_EQ(data2.role, ax::mojom::Role::kMenuItemRadio);

  item3->GetViewAccessibility().GetAccessibleNodeData(&data3);
  EXPECT_EQ(data3.role, ax::mojom::Role::kMenuItem);

  item4->GetViewAccessibility().GetAccessibleNodeData(&data4);
  EXPECT_EQ(data4.role, ax::mojom::Role::kMenuItem);
}

class TouchableMenuItemViewTest : public ViewsTestBase {
 public:
  TouchableMenuItemViewTest() = default;
  ~TouchableMenuItemViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->Show();

    menu_delegate_ = std::make_unique<test::TestMenuDelegate>();
    auto menu_item_view_owning =
        std::make_unique<TestMenuItemView>(menu_delegate_.get());
    menu_item_view_ = menu_item_view_owning.get();
    menu_runner_ = std::make_unique<MenuRunner>(
        std::move(menu_item_view_owning), MenuRunner::USE_ASH_SYS_UI_LAYOUT);
    menu_runner_->RunMenuAt(widget_.get(), nullptr, gfx::Rect(),
                            MenuAnchorPosition::kTopLeft,
                            ui::MENU_SOURCE_KEYBOARD);
  }

  void TearDown() override {
    widget_->CloseNow();
    ViewsTestBase::TearDown();
  }

  gfx::Size AppendItemAndGetSize(int i, const std::u16string& title) {
    return menu_item_view_->AppendMenuItem(i, title)->GetPreferredSize({});
  }

 private:
  std::unique_ptr<test::TestMenuDelegate> menu_delegate_;
  std::unique_ptr<MenuRunner> menu_runner_;
  std::unique_ptr<Widget> widget_;

  // Owned by MenuRunner.
  raw_ptr<TestMenuItemView> menu_item_view_ = nullptr;
};

// Test that touchable menu items are sized to fit the menu item titles within
// the allowed minimum and maximum width.
TEST_F(TouchableMenuItemViewTest, MinAndMaxWidth) {
  const int min_menu_width = MenuConfig::instance().touchable_menu_min_width;
  const int max_menu_width = MenuConfig::instance().touchable_menu_max_width;

  // Test a title shorter than the minimum width.
  gfx::Size item1_size = AppendItemAndGetSize(1, u"Item1 Short title");
  EXPECT_EQ(item1_size.width(), min_menu_width);

  // Test a title which is between the min and max allowed widths.
  gfx::Size item2_size =
      AppendItemAndGetSize(2, u"Item2 bigger than min less than max");
  EXPECT_GE(item2_size.width(), min_menu_width);
  EXPECT_LE(item2_size.width(), max_menu_width);

  // Test a title which is longer than the max touchable menu width.
  gfx::Size item3_size =
      AppendItemAndGetSize(3,
                           u"Item3 Title that is longer than the maximum "
                           u"allowed context menu width");
  EXPECT_EQ(item3_size.width(), max_menu_width);
}

class MenuItemViewLayoutTest : public ViewsTestBase {
 public:
  MenuItemViewLayoutTest() = default;
  ~MenuItemViewLayoutTest() override = default;

 protected:
  MenuItemView* test_item() { return test_item_; }

  void PerformLayout() {
    // SubmenuView does not lay out its children unless it is contained in a
    // view, so make a simple container for it.
    SubmenuView* submenu = root_menu_->GetSubmenu();
    ASSERT_TRUE(submenu->owned_by_client());

    submenu_parent_ = std::make_unique<View>();
    submenu_parent_->AddChildView(submenu);
    submenu_parent_->SetPosition(gfx::Point(0, 0));
    submenu_parent_->SetSize(submenu->GetPreferredSize({}));
  }

  void SetUp() override {
    ViewsTestBase::SetUp();
    root_menu_ = std::make_unique<TestMenuItemView>();
    test_item_ = root_menu_->AppendMenuItem(1);
  }

 private:
  std::unique_ptr<TestMenuItemView> root_menu_;
  raw_ptr<MenuItemView> test_item_ = nullptr;
  std::unique_ptr<View> submenu_parent_;
};

// Tests that MenuItemView takes into account the child's margins and preferred
// size when laying out in container mode.
TEST_F(MenuItemViewLayoutTest, ContainerLayoutRespectsMarginsAndPreferredSize) {
  // We make our menu item a simple container for our view.
  View* child_view = test_item()->AddChildView(std::make_unique<View>());

  // We want to check that MenuItemView::Layout() respects the child's preferred
  // size and margins.
  const gfx::Size child_size(200, 50);
  const auto child_margins = gfx::Insets::VH(5, 10);
  child_view->SetPreferredSize(child_size);
  child_view->SetProperty(kMarginsKey, child_margins);

  PerformLayout();

  // Get |child_view|'s bounds and check that they align with |child_view|'s
  // margins and preferred size.
  const gfx::Rect child_bounds = child_view->bounds();
  const gfx::Insets actual_margins =
      test_item()->GetContentsBounds().InsetsFrom(child_bounds);
  EXPECT_GE(actual_margins.left(), child_margins.left());
  EXPECT_GE(actual_margins.right(), child_margins.right());
  EXPECT_GE(actual_margins.top(), child_margins.top());
  EXPECT_GE(actual_margins.bottom(), child_margins.bottom());
  EXPECT_EQ(child_bounds.width(), child_size.width());
  EXPECT_EQ(child_bounds.height(), child_size.height());
}

class MenuItemViewPaintUnitTest : public ViewsTestBase {
 public:
  MenuItemViewPaintUnitTest() = default;

  MenuItemViewPaintUnitTest(const MenuItemViewPaintUnitTest&) = delete;
  MenuItemViewPaintUnitTest& operator=(const MenuItemViewPaintUnitTest&) =
      delete;

  ~MenuItemViewPaintUnitTest() override = default;

  MenuItemView* menu_item_view() { return menu_item_view_; }
  MenuRunner* menu_runner() { return menu_runner_.get(); }
  Widget* widget() { return widget_.get(); }

  // ViewsTestBase implementation.
  void SetUp() override {
    ViewsTestBase::SetUp();
    menu_delegate_ = CreateMenuDelegate();
    auto menu_item_view_owning =
        std::make_unique<MenuItemView>(menu_delegate_.get());
    menu_item_view_ = menu_item_view_owning.get();

    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
    widget_->Init(std::move(params));
    widget_->Show();

    menu_runner_ =
        std::make_unique<MenuRunner>(std::move(menu_item_view_owning), 0);
  }

  void TearDown() override {
    menu_item_view_ = nullptr;
    widget_->CloseNow();
    ViewsTestBase::TearDown();
  }

  test::TestMenuDelegate* GetDelegate() { return menu_delegate_.get(); }

  ax::mojom::CheckedState GetCheckedStatus(int command,
                                           views::MenuItemView::Type type) {
    if (type == views::MenuItemView::Type::kRadio ||
        type == views::MenuItemView::Type::kCheckbox) {
      bool is_checked = GetDelegate() && GetDelegate()->IsItemChecked(command);
      return is_checked ? ax::mojom::CheckedState::kTrue
                        : ax::mojom::CheckedState::kFalse;
    } else {
      return ax::mojom::CheckedState::kNone;
    }
  }

 protected:
  virtual std::unique_ptr<test::TestMenuDelegate> CreateMenuDelegate() {
    return std::make_unique<test::TestMenuDelegate>();
  }

 private:
  // Owned by MenuRunner.
  raw_ptr<MenuItemView> menu_item_view_ = nullptr;

  std::unique_ptr<test::TestMenuDelegate> menu_delegate_;
  std::unique_ptr<MenuRunner> menu_runner_;
  std::unique_ptr<Widget> widget_;
};

// Provides assertion coverage for painting, secondary label, minor text and
// icons.
TEST_F(MenuItemViewPaintUnitTest, MinorTextAndIconAssertionCoverage) {
  auto AddItem = [this](auto label, auto secondary_label, auto minor_label,
                        auto minor_icon) {
    menu_item_view()->AddMenuItemAt(0, 1000, label, secondary_label,
                                    minor_label, minor_icon, ui::ImageModel(),
                                    views::MenuItemView::Type::kNormal,
                                    ui::NORMAL_SEPARATOR);
  };
  AddItem(u"No secondary label, no minor content", std::u16string(),
          std::u16string(), ui::ImageModel());
  AddItem(u"No secondary label, minor text only", std::u16string(),
          u"minor text", ui::ImageModel());
  AddItem(u"No secondary label, minor icon only", std::u16string(),
          std::u16string(),
          ui::ImageModel::FromVectorIcon(views::kMenuCheckIcon));
  AddItem(u"No secondary label, minor text and icon", std::u16string(),
          u"minor text", ui::ImageModel::FromVectorIcon(views::kMenuCheckIcon));
  AddItem(u"Secondary label, no minor content", u"secondary label",
          std::u16string(), ui::ImageModel());
  AddItem(u"Secondary label, minor text only", u"secondary label",
          u"minor text", ui::ImageModel());
  AddItem(u"Secondary label, minor icon only", u"secondary label",
          std::u16string(),
          ui::ImageModel::FromVectorIcon(views::kMenuCheckIcon));
  AddItem(u"Secondary label, minor text and icon", u"secondary label",
          u"minor text", ui::ImageModel::FromVectorIcon(views::kMenuCheckIcon));

  menu_runner()->RunMenuAt(widget(), nullptr, gfx::Rect(),
                           MenuAnchorPosition::kTopLeft,
                           ui::MENU_SOURCE_KEYBOARD);

  SkBitmap bitmap;
  gfx::Size size = menu_item_view()->GetMirroredBounds().size();
  ui::CanvasPainter canvas_painter(&bitmap, size, 1.f, SK_ColorTRANSPARENT,
                                   false);
  menu_item_view()->GetSubmenu()->Paint(
      PaintInfo::CreateRootPaintInfo(canvas_painter.context(), size));
}

// Provides assertion coverage for painting with custom colors.
// icons.
TEST_F(MenuItemViewPaintUnitTest, CustomColorAssertionCoverage) {
  auto AddItem = [this](auto label, auto submenu_background_color,
                        auto foreground_color, auto selected_color) {
    menu_item_view()->AddMenuItemAt(
        0, 1000, label, std::u16string(), std::u16string(), ui::ImageModel(),
        ui::ImageModel(), views::MenuItemView::Type::kNormal,
        ui::NORMAL_SEPARATOR, submenu_background_color, foreground_color,
        selected_color);
  };
  ui::ColorId background_color = ui::kColorComboboxBackground;
  ui::ColorId foreground_color = ui::kColorDropdownForeground;
  ui::ColorId selected_color = ui::kColorMenuItemForegroundHighlighted;
  AddItem(u"No custom colors", std::nullopt, std::nullopt, std::nullopt);
  AddItem(u"No selected color", background_color, foreground_color,
          std::nullopt);
  AddItem(u"No foreground color", background_color, std::nullopt,
          selected_color);
  AddItem(u"No background color", std::nullopt, foreground_color,
          selected_color);
  AddItem(u"No background or foreground", std::nullopt, std::nullopt,
          selected_color);
  AddItem(u"No background or selected", std::nullopt, foreground_color,
          std::nullopt);
  AddItem(u"No foreground or selected", background_color, std::nullopt,
          std::nullopt);
  AddItem(u"All colors", background_color, foreground_color, selected_color);

  menu_runner()->RunMenuAt(widget(), nullptr, gfx::Rect(),
                           MenuAnchorPosition::kTopLeft,
                           ui::MENU_SOURCE_KEYBOARD);

  SkBitmap bitmap;
  gfx::Size size = menu_item_view()->GetMirroredBounds().size();
  ui::CanvasPainter canvas_painter(&bitmap, size, 1.f, SK_ColorTRANSPARENT,
                                   false);
  menu_item_view()->GetSubmenu()->Paint(
      PaintInfo::CreateRootPaintInfo(canvas_painter.context(), size));
}

// Verifies a call to MenuItemView::OnPaint() doesn't trigger a call to
// MenuItemView::submenu_arrow_image_view_::SchedulePaint(). This is a
// regression test for https://crbug.com/1245854.
TEST_F(MenuItemViewPaintUnitTest, DontSchedulePaintFromOnPaint) {
  MenuItemView* submenu_item =
      menu_item_view()->AppendSubMenu(1, u"My Submenu");
  submenu_item->AppendMenuItem(1, u"submenu item 1");

  menu_runner()->RunMenuAt(widget(), nullptr, gfx::Rect(),
                           MenuAnchorPosition::kTopLeft,
                           ui::MENU_SOURCE_KEYBOARD);

  ImageView* submenu_arrow_image_view =
      TestMenuItemView::submenu_arrow_image_view(submenu_item);
  ASSERT_TRUE(submenu_arrow_image_view);
  ViewTestApi(submenu_arrow_image_view).ClearNeedsPaint();

  // Paint again. As no state has changed since the last paint, this should not
  // call SchedulePaint() on the `submenu_arrow_image_view`
  gfx::Canvas canvas(submenu_item->size(), 1.f, false /* opaque */);
  submenu_item->OnPaint(&canvas);
  EXPECT_FALSE(ViewTestApi(submenu_arrow_image_view).needs_paint());
}

// Tests to ensure that selection based state is not updated if a menu item or
// an anscestor item has been scheduled for deletion. This guards against
// removed but not-yet-deleted MenuItemViews using stale model data to update
// state.
TEST_F(MenuItemViewPaintUnitTest,
       SelectionBasedStateNotUpdatedWhenScheduledForDeletion) {
  MenuItemView* submenu_item =
      menu_item_view()->AppendSubMenu(1, u"submenu_item");
  MenuItemView* submenu_child =
      submenu_item->AppendMenuItem(1, u"submenu_child");

  menu_runner()->RunMenuAt(widget(), nullptr, gfx::Rect(),
                           MenuAnchorPosition::kTopLeft,
                           ui::MENU_SOURCE_KEYBOARD);

  // Show both the root and nested menus.
  SubmenuView* submenu = submenu_item->GetSubmenu();
  MenuHost::InitParams params;
  params.parent = widget();
  params.bounds = submenu_item->bounds();
  submenu->ShowAt(params);

  // The selected bit and selection based state should both update for all menu
  // items while they and their anscestors remain part of the menu.
  EXPECT_FALSE(submenu_item->IsSelected());
  EXPECT_FALSE(submenu_item->last_paint_as_selected_for_testing());
  submenu_item->SetSelected(true);
  EXPECT_TRUE(submenu_item->IsSelected());
  EXPECT_TRUE(submenu_item->last_paint_as_selected_for_testing());
  submenu_item->SetSelected(false);
  EXPECT_FALSE(submenu_item->IsSelected());
  EXPECT_FALSE(submenu_item->last_paint_as_selected_for_testing());

  EXPECT_FALSE(submenu_child->IsSelected());
  EXPECT_FALSE(submenu_child->last_paint_as_selected_for_testing());
  submenu_child->SetSelected(true);
  EXPECT_TRUE(submenu_child->IsSelected());
  EXPECT_TRUE(submenu_child->last_paint_as_selected_for_testing());
  submenu_child->SetSelected(false);
  EXPECT_FALSE(submenu_child->IsSelected());
  EXPECT_FALSE(submenu_child->last_paint_as_selected_for_testing());

  // Remove the child items from the root.
  menu_item_view()->RemoveAllMenuItems();

  // The selected bit should still update but selection based state, proxied by
  // `last_paint_as_selected_`, should remain unchanged.
  submenu_item->SetSelected(true);
  EXPECT_TRUE(submenu_item->IsSelected());
  EXPECT_FALSE(submenu_item->last_paint_as_selected_for_testing());
  submenu_item->SetSelected(false);
  EXPECT_FALSE(submenu_item->IsSelected());
  EXPECT_FALSE(submenu_item->last_paint_as_selected_for_testing());

  submenu_child->SetSelected(true);
  EXPECT_TRUE(submenu_child->IsSelected());
  EXPECT_FALSE(submenu_child->last_paint_as_selected_for_testing());
  submenu_child->SetSelected(false);
  EXPECT_FALSE(submenu_child->IsSelected());
  EXPECT_FALSE(submenu_child->last_paint_as_selected_for_testing());
}

TEST_F(MenuItemViewPaintUnitTest, SelectionBasedStateUpdatedWhenIconChanges) {
  MenuItemView* child_menu_item =
      menu_item_view()->AppendMenuItem(1, u"menu item");

  menu_runner()->RunMenuAt(widget(), nullptr, gfx::Rect(),
                           MenuAnchorPosition::kTopLeft,
                           ui::MENU_SOURCE_KEYBOARD);

  EXPECT_FALSE(child_menu_item->last_paint_as_selected_for_testing());
  child_menu_item->SetSelected(true);
  EXPECT_TRUE(child_menu_item->IsSelected());
  EXPECT_TRUE(child_menu_item->last_paint_as_selected_for_testing());

  child_menu_item->SetIconView(std::make_unique<ImageView>());
  EXPECT_TRUE(child_menu_item->IsSelected());
  EXPECT_TRUE(child_menu_item->last_paint_as_selected_for_testing());
}

TEST_F(MenuItemViewPaintUnitTest, SelectionBasedStateUpdatedDuringDragAndDrop) {
  MenuItemView* submenu_item =
      menu_item_view()->AppendSubMenu(1, u"submenu_item");
  MenuItemView* submenu_child1 =
      submenu_item->AppendMenuItem(1, u"submenu_child");
  MenuItemView* submenu_child2 =
      submenu_item->AppendMenuItem(1, u"submenu_child");

  menu_runner()->RunMenuAt(widget(), nullptr, gfx::Rect(),
                           MenuAnchorPosition::kTopLeft,
                           ui::MENU_SOURCE_KEYBOARD);

  // Show both the root and nested menus.
  SubmenuView* submenu = submenu_item->GetSubmenu();
  MenuHost::InitParams params;
  params.parent = widget();
  params.bounds = submenu_item->bounds();
  submenu->ShowAt(params);

  EXPECT_FALSE(submenu_child1->last_paint_as_selected_for_testing());
  submenu_child1->SetSelected(true);
  EXPECT_TRUE(submenu_child1->IsSelected());
  EXPECT_TRUE(submenu_child1->last_paint_as_selected_for_testing());

  // Setting the drop item to `submenu_child2` should force `submenu_child1` to
  // no longer draw as selected.
  submenu->SetDropMenuItem(submenu_child2, MenuDelegate::DropPosition::kOn);
  EXPECT_FALSE(submenu_child1->last_paint_as_selected_for_testing());
  EXPECT_FALSE(submenu_child2->last_paint_as_selected_for_testing());

  // Clearing the drop item returns `submenu_child1` to drawing as selected.
  submenu->SetDropMenuItem(nullptr, MenuDelegate::DropPosition::kOn);
  EXPECT_TRUE(submenu_child1->last_paint_as_selected_for_testing());
  EXPECT_FALSE(submenu_child2->last_paint_as_selected_for_testing());
}

TEST_F(MenuItemViewPaintUnitTest, AccessibleCheckedStateChange) {
  int command = 1000;
  auto type = views::MenuItemView::Type::kNormal;
  ui::AXNodeData data;

  auto AddItem = [this](auto command_, auto type_) {
    menu_item_view()->AddMenuItemAt(
        0, command_, u"No custom colors", std::u16string(), std::u16string(),
        ui::ImageModel(), ui::ImageModel(), type_, ui::NORMAL_SEPARATOR,
        std::nullopt, std::nullopt, std::nullopt);
  };

  type = views::MenuItemView::Type::kRadio;
  AddItem(command, type);
  menu_item_view()
      ->GetMenuItemByID(command)
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), GetCheckedStatus(command, type));
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);

  data = ui::AXNodeData();
  type = views::MenuItemView::Type::kCheckbox;
  AddItem(command, type);
  menu_item_view()
      ->GetMenuItemByID(command)
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), GetCheckedStatus(command, type));
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);

  data = ui::AXNodeData();
  type = views::MenuItemView::Type::kNormal;
  AddItem(command, type);
  menu_item_view()
      ->GetMenuItemByID(command)
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), GetCheckedStatus(command, type));
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kNone);

  data = ui::AXNodeData();
  type = views::MenuItemView::Type::kSubMenu;
  AddItem(command, type);
  menu_item_view()
      ->GetMenuItemByID(command)
      ->GetSubmenu()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), GetCheckedStatus(command, type));
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kNone);
}

TEST_F(MenuItemViewPaintUnitTest, AccessibleHasPopup) {
  int command = 1000;
  auto type = views::MenuItemView::Type::kNormal;
  ui::AXNodeData data1, data2;

  auto AddItem = [this](auto command_, auto type_) {
    menu_item_view()->AddMenuItemAt(
        0, command_, u"No custom colors", std::u16string(), std::u16string(),
        ui::ImageModel(), ui::ImageModel(), type_, ui::NORMAL_SEPARATOR,
        std::nullopt, std::nullopt, std::nullopt);
  };

  type = views::MenuItemView::Type::kCheckbox;
  AddItem(command, type);
  menu_item_view()
      ->GetMenuItemByID(command)
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data1);
  EXPECT_FALSE(data1.HasIntAttribute(ax::mojom::IntAttribute::kHasPopup));

  data1 = ui::AXNodeData();
  type = views::MenuItemView::Type::kSubMenu;
  AddItem(command, type);
  menu_item_view()
      ->GetMenuItemByID(command)
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data1);
  EXPECT_EQ(data1.GetHasPopup(), ax::mojom::HasPopup::kMenu);

  menu_item_view()
      ->GetMenuItemByID(command)
      ->GetSubmenu()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data2);
  EXPECT_EQ(data2.GetHasPopup(), ax::mojom::HasPopup::kMenu);
}

// Sets up a custom MenuDelegate that expects functions aren't called. See
// DontAskForFontsWhenAddingSubmenu.
class MenuItemViewAccessTest : public MenuItemViewPaintUnitTest {
 public:
 protected:
  std::unique_ptr<test::TestMenuDelegate> CreateMenuDelegate() override {
    return std::make_unique<DisallowMenuDelegate>();
  }

 private:
  class DisallowMenuDelegate : public test::TestMenuDelegate {
   public:
    std::optional<SkColor> GetLabelColor(int command_id) const override {
      EXPECT_NE(1, command_id);
      return std::nullopt;
    }
  };
};

// Verifies AppendSubMenu() doesn't trigger calls to the delegate with the
// command being supplied. The delegate can be called after AppendSubMenu(),
// but not before.
TEST_F(MenuItemViewAccessTest, DontAskForFontsWhenAddingSubmenu) {
  menu_item_view()->AppendSubMenu(1, u"My Submenu");
}

using MenuItemViewA11yTest = MenuItemViewPaintUnitTest;

// A MenuItemView that has a submenu should open the submenu on kExpand and
// close the submenu on kCollapse.
TEST_F(MenuItemViewA11yTest, HandlesExpandCollapseActions) {
  MenuItemView* submenu_item_view =
      menu_item_view()->AppendSubMenu(1, u"Submenu");
  menu_runner()->RunMenuAt(widget(), nullptr, gfx::Rect(),
                           MenuAnchorPosition::kTopLeft,
                           ui::MENU_SOURCE_KEYBOARD);

  // Pre-conditions: An expandable submenu item.
  ASSERT_TRUE(submenu_item_view->HasSubmenu());
  ASSERT_FALSE(submenu_item_view->SubmenuIsShowing());

  // Send an expand action to the menu item.
  ui::AXActionData expand_action_data;
  expand_action_data.action = ax::mojom::Action::kExpand;
  submenu_item_view->HandleAccessibleAction(expand_action_data);
  EXPECT_TRUE(submenu_item_view->SubmenuIsShowing());

  // Send a collapse action to the menu item.
  ui::AXActionData collapse_action_data;
  collapse_action_data.action = ax::mojom::Action::kCollapse;
  submenu_item_view->HandleAccessibleAction(collapse_action_data);
  EXPECT_FALSE(submenu_item_view->SubmenuIsShowing());
}

TEST_F(MenuItemViewA11yTest, AccessibleSelectedTest) {
  MenuItemView* item = menu_item_view();
  ui::AXNodeData data;
  item->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_TRUE(data.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  item->SetSelected(true);
  data = ui::AXNodeData();
  item->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  item->SetSelected(false);
  data = ui::AXNodeData();
  item->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  item->SetSelected(true);
  item->SetEnabled(false);
  data = ui::AXNodeData();
  item->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  item->SetEnabled(true);
  item->SetVisible(false);
  data = ui::AXNodeData();
  item->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  item->SetVisible(true);
  data = ui::AXNodeData();
  item->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

}  // namespace views
