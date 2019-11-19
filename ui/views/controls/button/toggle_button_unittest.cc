// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/toggle_button.h"

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"

namespace views {

class TestToggleButton : public ToggleButton {
 public:
  explicit TestToggleButton(int* counter)
      : ToggleButton(nullptr), counter_(counter) {}
  ~TestToggleButton() override {
    // Calling SetInkDropMode() in this subclass allows this class's
    // implementation of RemoveInkDropLayer() to be called. The same
    // call is made in ~ToggleButton() so this is testing the general technique.
    SetInkDropMode(InkDropMode::OFF);
  }

  using View::Focus;

 protected:
  // ToggleButton:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override {
    ++(*counter_);
    ToggleButton::AddInkDropLayer(ink_drop_layer);
  }

  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override {
    ToggleButton::RemoveInkDropLayer(ink_drop_layer);
    --(*counter_);
  }

 private:
  int* counter_;

  DISALLOW_COPY_AND_ASSIGN(TestToggleButton);
};

class ToggleButtonTest : public ViewsTestBase {
 public:
  ToggleButtonTest() = default;
  ~ToggleButtonTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a widget so that the ToggleButton can query the hover state
    // correctly.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();

    button_ = new TestToggleButton(&counter_);
    widget_->SetContentsView(button_);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  int counter() const { return counter_; }
  Widget* widget() { return widget_.get(); }
  TestToggleButton* button() { return button_; }

 private:
  std::unique_ptr<Widget> widget_;
  TestToggleButton* button_ = nullptr;
  int counter_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ToggleButtonTest);
};

// Starts ink drop animation on a ToggleButton and destroys the button.
// The test verifies that the ink drop layer is removed properly when the
// ToggleButton gets destroyed.
TEST_F(ToggleButtonTest, ToggleButtonDestroyed) {
  EXPECT_EQ(0, counter());
  gfx::Point center(10, 10);
  button()->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(1, counter());
  delete button();
  EXPECT_EQ(0, counter());
}

// Make sure nothing bad happens when the widget is destroyed while the
// ToggleButton has focus (and is showing a ripple).
TEST_F(ToggleButtonTest, ShutdownWithFocus) {
  button()->RequestFocus();
  EXPECT_EQ(1, counter());
}

// Verify that ToggleButton::accepts_events_ works as expected.
TEST_F(ToggleButtonTest, AcceptEvents) {
  EXPECT_FALSE(button()->GetIsOn());
  ui::test::EventGenerator generator(GetRootWindow(widget()));

  // Clicking toggles.
  generator.ClickLeftButton();
  EXPECT_TRUE(button()->GetIsOn());
  generator.ClickLeftButton();
  EXPECT_FALSE(button()->GetIsOn());

  // Spacebar toggles.
  button()->RequestFocus();
  generator.PressKey(ui::VKEY_SPACE, ui::EF_NONE);
  generator.ReleaseKey(ui::VKEY_SPACE, ui::EF_NONE);
  EXPECT_TRUE(button()->GetIsOn());
  generator.PressKey(ui::VKEY_SPACE, ui::EF_NONE);
  generator.ReleaseKey(ui::VKEY_SPACE, ui::EF_NONE);
  EXPECT_FALSE(button()->GetIsOn());

  // Spacebar and clicking do nothing when not accepting events, but focus is
  // not affected.
  button()->SetAcceptsEvents(false);
  EXPECT_TRUE(button()->HasFocus());
  generator.PressKey(ui::VKEY_SPACE, ui::EF_NONE);
  generator.ReleaseKey(ui::VKEY_SPACE, ui::EF_NONE);
  EXPECT_FALSE(button()->GetIsOn());
  generator.ClickLeftButton();
  EXPECT_FALSE(button()->GetIsOn());

  // Re-enable events and clicking and spacebar resume working.
  button()->SetAcceptsEvents(true);
  generator.PressKey(ui::VKEY_SPACE, ui::EF_NONE);
  generator.ReleaseKey(ui::VKEY_SPACE, ui::EF_NONE);
  EXPECT_TRUE(button()->GetIsOn());
  generator.ClickLeftButton();
  EXPECT_FALSE(button()->GetIsOn());
}

}  // namespace views
