// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/radio_button.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/views_test_base.h"

namespace {
// Group ID of the test radio buttons.
constexpr int kGroup = 1;
}  // namespace

namespace views {

class RadioButtonTest : public ViewsTestBase {
 public:
  RadioButtonTest() = default;

  RadioButtonTest(const RadioButtonTest&) = delete;
  RadioButtonTest& operator=(const RadioButtonTest&) = delete;

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a Widget so the radio buttons can find their group siblings.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget_->Init(std::move(params));
    widget_->Show();

    button_container_ = widget_->SetContentsView(std::make_unique<View>());
  }

  void TearDown() override {
    button_container_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  View& button_container() { return *button_container_; }

 private:
  raw_ptr<View> button_container_ = nullptr;
  std::unique_ptr<Widget> widget_;
};

TEST_F(RadioButtonTest, Basics) {
  RadioButton* button1 = new RadioButton(u"Blah", kGroup);
  button_container().AddChildView(button1);
  RadioButton* button2 = new RadioButton(u"Blah", kGroup);
  button_container().AddChildView(button2);

  button1->SetChecked(true);
  EXPECT_TRUE(button1->GetChecked());
  EXPECT_FALSE(button2->GetChecked());

  button2->SetChecked(true);
  EXPECT_FALSE(button1->GetChecked());
  EXPECT_TRUE(button2->GetChecked());
}

TEST_F(RadioButtonTest, Focus) {
  RadioButton* button1 = new RadioButton(u"Blah", kGroup);
  button_container().AddChildView(button1);
  RadioButton* button2 = new RadioButton(u"Blah", kGroup);
  button_container().AddChildView(button2);

  // Tabbing through only focuses the checked button.
  button1->SetChecked(true);
  auto* focus_manager = button_container().GetFocusManager();
  ui::KeyEvent pressed_tab(ui::EventType::kKeyPressed, ui::VKEY_TAB,
                           ui::EF_NONE);
  focus_manager->OnKeyEvent(pressed_tab);
  EXPECT_EQ(button1, focus_manager->GetFocusedView());
  focus_manager->OnKeyEvent(pressed_tab);
  EXPECT_EQ(button1, focus_manager->GetFocusedView());

  // The checked button can be moved using arrow keys.
  focus_manager->OnKeyEvent(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_DOWN, ui::EF_NONE));
  EXPECT_EQ(button2, focus_manager->GetFocusedView());
  EXPECT_FALSE(button1->GetChecked());
  EXPECT_TRUE(button2->GetChecked());

  focus_manager->OnKeyEvent(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_UP, ui::EF_NONE));
  EXPECT_EQ(button1, focus_manager->GetFocusedView());
  EXPECT_TRUE(button1->GetChecked());
  EXPECT_FALSE(button2->GetChecked());
}

TEST_F(RadioButtonTest, FocusOnClick) {
  RadioButton* button1 = new RadioButton(std::u16string(), kGroup);
  button1->SetSize(gfx::Size(10, 10));
  button_container().AddChildView(button1);
  button1->SetChecked(true);
  RadioButton* button2 = new RadioButton(std::u16string(), kGroup);
  button2->SetSize(gfx::Size(10, 10));
  button_container().AddChildView(button2);

  const gfx::Point point(1, 1);
  const ui::MouseEvent event(ui::EventType::kMousePressed, point, point,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  button2->OnMousePressed(event);
  button2->OnMouseReleased(event);

  EXPECT_TRUE(button2->GetChecked());
  auto* focus_manager = button_container().GetFocusManager();
  // No focus on click.
  EXPECT_EQ(nullptr, focus_manager->GetFocusedView());

  ui::KeyEvent pressed_tab(ui::EventType::kKeyPressed, ui::VKEY_TAB,
                           ui::EF_NONE);
  focus_manager->OnKeyEvent(pressed_tab);
  EXPECT_EQ(button2, focus_manager->GetFocusedView());

  button1->OnMousePressed(event);
  button1->OnMouseReleased(event);
  // Button 1 gets focus on click because button 2 already had it.
  EXPECT_TRUE(button1->GetChecked());
  EXPECT_EQ(button1, focus_manager->GetFocusedView());
}

}  // namespace views
