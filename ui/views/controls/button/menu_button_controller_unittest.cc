// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/menu_button_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

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

// Verify DecrementPressedLocked doesn't crash if the controller is destroyed
// during SetState(). This can happen on macOS when a state-change callback
// cascade closes a widget and tears down the controller re-entrantly.
TEST_F(MenuButtonControllerTest,
       DecrementPressedLockedSurvivesControllerDestructionDuringSetState) {
  auto* controller =
      static_cast<MenuButtonController*>(button()->button_controller());

  auto lock = controller->TakeLock();
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());

  // When the button state changes, replace the controller. This destroys the
  // old controller while DecrementPressedLocked is still on the stack.
  base::CallbackListSubscription subscription =
      button()->AddStateChangedCallback(base::BindRepeating(
          [](Button* btn) {
            btn->SetButtonController(std::make_unique<MenuButtonController>(
                btn, Button::PressedCallback(),
                std::make_unique<Button::DefaultButtonControllerDelegate>(
                    btn)));
          },
          button()));

  // Releasing the lock triggers old controller destroyed.
  // The WeakPtr check prevents the use-after-free.
  lock.reset();
}

}  // namespace views
