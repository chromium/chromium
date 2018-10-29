// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/submenu_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace test {

namespace {

// MenuDelegate mock to control the CreateFootnoteView() method.
class MockMenuDelegate : public MenuDelegate {
 public:
  MockMenuDelegate() = default;
  ~MockMenuDelegate() override = default;

  void set_create_footnote_view_value(View* view) {
    create_footnote_view_value_ = view;
  }
  int create_footnote_view_count() { return create_footnote_view_count_; }

  // MenuDelegate:
  View* CreateFootnoteView() override {
    create_footnote_view_count_++;
    return create_footnote_view_value_;
  }

 private:
  // The return value for the next CreateFootnoteView call.
  View* create_footnote_view_value_ = nullptr;

  // The number of times CreateFootnoteView was called.
  int create_footnote_view_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MockMenuDelegate);
};

}  // namespace

class MenuFootnoteTest : public ViewsTestBase {
 public:
  MenuFootnoteTest();
  ~MenuFootnoteTest() override;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    menu_delegate_ = std::make_unique<MockMenuDelegate>();
    menu_item_view_ = new MenuItemView(menu_delegate_.get());
    item_with_submenu_ = menu_item_view_->AppendSubMenu(0, base::string16());

    owner_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    owner_->Init(params);
    owner_->Show();

    menu_runner_ = std::make_unique<MenuRunner>(menu_item_view_, 0);
  }

  void TearDown() override {
    if (owner_)
      owner_->CloseNow();
    ViewsTestBase::TearDown();
  }

 protected:
  // Owned by menu_runner_.
  MenuItemView* menu_item_view_ = nullptr;

  // An item with a submenu, in menu_item_view_.
  MenuItemView* item_with_submenu_ = nullptr;

  std::unique_ptr<MockMenuDelegate> menu_delegate_;
  std::unique_ptr<MenuRunner> menu_runner_;
  std::unique_ptr<Widget> owner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuFootnoteTest);
};

MenuFootnoteTest::MenuFootnoteTest() = default;

MenuFootnoteTest::~MenuFootnoteTest() = default;

TEST_F(MenuFootnoteTest, TopLevelContainerShowsFootnote) {
  View* footnote = new View();
  menu_delegate_->set_create_footnote_view_value(footnote);
  menu_runner_->RunMenuAt(owner_.get(), nullptr, gfx::Rect(),
                          MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_NONE);
  EXPECT_EQ(1, menu_delegate_->create_footnote_view_count());
  EXPECT_TRUE(menu_item_view_->GetSubmenu()->Contains(footnote));
}

TEST_F(MenuFootnoteTest, SubmenuDoesNotShowFootnote) {
  View* footnote = new View();
  menu_delegate_->set_create_footnote_view_value(footnote);
  menu_runner_->RunMenuAt(owner_.get(), nullptr, gfx::Rect(),
                          MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_NONE);
  // Trigger the code that would create a footnote, then check that the footnote
  // was not created.
  item_with_submenu_->GetSubmenu()->GetScrollViewContainer();
  EXPECT_FALSE(item_with_submenu_->GetSubmenu()->Contains(footnote));
  EXPECT_EQ(1, menu_delegate_->create_footnote_view_count());
}

}  // namespace test
}  // namespace views
