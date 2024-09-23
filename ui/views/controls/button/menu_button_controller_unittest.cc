// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/menu_button_controller.h"
#include "base/functional/bind.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/views_test_base.h"

namespace views {

class TestButton : public Button {
 public:
  TestButton() : Button(PressedCallback()) {
    auto menu_button_controller = std::make_unique<MenuButtonController>(
        this,
        base::BindRepeating([](bool* pressed) { *pressed = true; }, &pressed_),
        std::make_unique<views::Button::DefaultButtonControllerDelegate>(this));
    SetButtonController(std::move(menu_button_controller));
  }

  bool pressed() const { return pressed_; }

 private:
  bool pressed_ = false;
};

class MenuButtonControllerTest : public ViewsTestBase {
 public:
  MenuButtonControllerTest() = default;

  MenuButtonControllerTest(const MenuButtonControllerTest&) = delete;
  MenuButtonControllerTest& operator=(const MenuButtonControllerTest&) = delete;

  ~MenuButtonControllerTest() override = default;

 protected:
  Widget* widget() { return widget_.get(); }
  TestButton* button() {
    return static_cast<TestButton*>(widget()->GetContentsView());
  }

 private:
  std::unique_ptr<Widget> widget_;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();
    widget()->SetContentsView(std::make_unique<TestButton>());
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }
};

TEST_F(MenuButtonControllerTest, NotifyClickInvokePressedCallback) {
  EXPECT_FALSE(button()->pressed());
  button()->button_controller()->NotifyClick();
  EXPECT_TRUE(button()->pressed());
}

}  // namespace views
