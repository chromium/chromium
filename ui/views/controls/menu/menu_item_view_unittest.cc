// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_item_view.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/vector_icons.h"

namespace views {

namespace {

// A simple View class that will match its height to the available width.
class SquareView : public views::View {
 public:
  SquareView() {}
  ~SquareView() override {}

 private:
  gfx::Size CalculatePreferredSize() const override { return gfx::Size(1, 1); }
  int GetHeightForWidth(int width) const override { return width; }
};

}  // namespace

// A MenuItemView implementation with a public destructor (so we can clean up
// in tests).
class TestMenuItemView : public MenuItemView {
 public:
  TestMenuItemView() : MenuItemView(NULL) {}
  ~TestMenuItemView() override {}

  void AddEmptyMenus() { MenuItemView::AddEmptyMenus(); }

  void SetHasMnemonics(bool has_mnemonics) { has_mnemonics_ = has_mnemonics; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMenuItemView);
};

TEST(MenuItemViewUnitTest, TestMenuItemViewWithFlexibleWidthChild) {
  TestMenuItemView root_menu;
  root_menu.set_owned_by_client();

  // Append a normal MenuItemView.
  views::MenuItemView* label_view =
      root_menu.AppendMenuItemWithLabel(1, base::ASCIIToUTF16("item 1"));

  // Append a second MenuItemView that has a child SquareView.
  views::MenuItemView* flexible_view =
      root_menu.AppendMenuItemWithLabel(2, base::string16());
  flexible_view->AddChildView(new SquareView());
  // Set margins to 0 so that we know width should match height.
  flexible_view->SetMargins(0, 0);

  views::SubmenuView* submenu = root_menu.GetSubmenu();

  // The first item should be the label view.
  ASSERT_EQ(label_view, submenu->GetMenuItemAt(0));
  gfx::Size label_size = label_view->GetPreferredSize();

  // The second item should be the flexible view.
  ASSERT_EQ(flexible_view, submenu->GetMenuItemAt(1));
  gfx::Size flexible_size = flexible_view->GetPreferredSize();

  EXPECT_EQ(1, flexible_size.width());

  // ...but it should use whatever space is available to make a square.
  int flex_height = flexible_view->GetHeightForWidth(label_size.width());
  EXPECT_EQ(label_size.width(), flex_height);

  // The submenu should be tall enough to allow for both menu items at the
  // given width.
  EXPECT_EQ(label_size.height() + flex_height,
            submenu->GetPreferredSize().height());
}

// Tests that the top-level menu item with hidden children should contain the
// "(empty)" menu item to display.
TEST(MenuItemViewUnitTest, TestEmptyTopLevelWhenAllItemsAreHidden) {
  TestMenuItemView root_menu;
  views::MenuItemView* item1 =
      root_menu.AppendMenuItemWithLabel(1, base::ASCIIToUTF16("item 1"));
  views::MenuItemView* item2 =
      root_menu.AppendMenuItemWithLabel(2, base::ASCIIToUTF16("item 2"));

  // Set menu items to hidden.
  item1->SetVisible(false);
  item2->SetVisible(false);

  SubmenuView* submenu = root_menu.GetSubmenu();
  ASSERT_TRUE(submenu);

  EXPECT_EQ(2, submenu->child_count());

  // Adds any empty menu items to the menu, if needed.
  root_menu.AddEmptyMenus();

  // Because all of the submenu's children are hidden, an empty menu item should
  // have been added.
  ASSERT_EQ(3, submenu->child_count());
  MenuItemView* empty_item = static_cast<MenuItemView*>(submenu->child_at(0));
  ASSERT_TRUE(empty_item);
  ASSERT_EQ(MenuItemView::kEmptyMenuItemViewID, empty_item->id());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_MENU_EMPTY_SUBMENU),
            empty_item->title());
}

// Tests that submenu with hidden children should contain the "(empty)" menu
// item to display.
TEST(MenuItemViewUnitTest, TestEmptySubmenuWhenAllChildItemsAreHidden) {
  TestMenuItemView root_menu;
  MenuItemView* submenu_item =
      root_menu.AppendSubMenu(1, base::ASCIIToUTF16("My Submenu"));
  MenuItemView* child1 = submenu_item->AppendMenuItemWithLabel(
      1, base::ASCIIToUTF16("submenu item 1"));
  MenuItemView* child2 = submenu_item->AppendMenuItemWithLabel(
      2, base::ASCIIToUTF16("submenu item 2"));

  // Set submenu children to hidden.
  child1->SetVisible(false);
  child2->SetVisible(false);

  SubmenuView* submenu = submenu_item->GetSubmenu();
  ASSERT_TRUE(submenu);

  EXPECT_EQ(2, submenu->child_count());

  // Adds any empty menu items to the menu, if needed.
  EXPECT_FALSE(submenu->HasEmptyMenuItemView());
  root_menu.AddEmptyMenus();
  EXPECT_TRUE(submenu->HasEmptyMenuItemView());
  // Because all of the submenu's children are hidden, an empty menu item should
  // have been added.
  ASSERT_EQ(3, submenu->child_count());
  MenuItemView* empty_item = static_cast<MenuItemView*>(submenu->child_at(0));
  ASSERT_TRUE(empty_item);
  // Not allowed to add an duplicated empty menu item
  // if it already has an empty menu item.
  root_menu.AddEmptyMenus();
  ASSERT_EQ(3, submenu->child_count());
  ASSERT_EQ(MenuItemView::kEmptyMenuItemViewID, empty_item->id());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_MENU_EMPTY_SUBMENU),
            empty_item->title());
}

TEST(MenuItemViewUnitTest, UseMnemonicOnPlatform) {
  TestMenuItemView root_menu;
  views::MenuItemView* item1 =
      root_menu.AppendMenuItemWithLabel(1, base::ASCIIToUTF16("&Item 1"));
  views::MenuItemView* item2 =
      root_menu.AppendMenuItemWithLabel(2, base::ASCIIToUTF16("I&tem 2"));

  root_menu.SetHasMnemonics(true);

  if (MenuConfig::instance().use_mnemonics) {
    EXPECT_EQ('i', item1->GetMnemonic());
    EXPECT_EQ('t', item2->GetMnemonic());
  } else {
    EXPECT_EQ(0, item1->GetMnemonic());
    EXPECT_EQ(0, item2->GetMnemonic());
  }
}

class MenuItemViewPaintUnitTest : public ViewsTestBase {
 public:
  MenuItemViewPaintUnitTest() {}
  ~MenuItemViewPaintUnitTest() override {}

  MenuItemView* menu_item_view() { return menu_item_view_; }
  MenuRunner* menu_runner() { return menu_runner_.get(); }
  Widget* widget() { return widget_.get(); }

  // ViewsTestBase implementation.
  void SetUp() override {
    ViewsTestBase::SetUp();
    menu_delegate_.reset(new test::TestMenuDelegate);
    menu_item_view_ = new MenuItemView(menu_delegate_.get());

    widget_.reset(new Widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_->Init(params);
    widget_->Show();

    menu_runner_.reset(new MenuRunner(menu_item_view_, 0));
  }

  void TearDown() override {
    widget_->CloseNow();
    ViewsTestBase::TearDown();
  }

 private:
  // Owned by MenuRunner.
  MenuItemView* menu_item_view_;

  std::unique_ptr<test::TestMenuDelegate> menu_delegate_;
  std::unique_ptr<MenuRunner> menu_runner_;
  std::unique_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(MenuItemViewPaintUnitTest);
};

// Provides assertion coverage for painting minor text and icons.
TEST_F(MenuItemViewPaintUnitTest, MinorTextAndIconAssertionCoverage) {
  auto AddItem = [this](auto label, auto minor_label, auto minor_icon) {
    menu_item_view()->AddMenuItemAt(
        0, 1000, base::ASCIIToUTF16(label), base::string16(), minor_label,
        minor_icon, gfx::ImageSkia(), views::MenuItemView::NORMAL,
        ui::NORMAL_SEPARATOR);
  };
  AddItem("No minor content", base::string16(), nullptr);
  AddItem("Minor text only", base::ASCIIToUTF16("minor text"), nullptr);
  AddItem("Minor icon only", base::string16(), &views::kMenuCheckIcon);
  AddItem("Minor text and icon", base::ASCIIToUTF16("minor text"),
          &views::kMenuCheckIcon);

  menu_runner()->RunMenuAt(widget(), nullptr, gfx::Rect(), MENU_ANCHOR_TOPLEFT,
                           ui::MENU_SOURCE_KEYBOARD);

  SkBitmap bitmap;
  gfx::Size size = menu_item_view()->GetMirroredBounds().size();
  ui::CanvasPainter canvas_painter(&bitmap, size, 1.f, SK_ColorTRANSPARENT,
                                   false);
  menu_item_view()->GetSubmenu()->Paint(
      PaintInfo::CreateRootPaintInfo(canvas_painter.context(), size));
}

}  // namespace views
