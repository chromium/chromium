// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/toggle_button.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"

namespace views {

class TestToggleButton : public ToggleButton {
 public:
  explicit TestToggleButton(int* counter) : counter_(counter) {}

  TestToggleButton(const TestToggleButton&) = delete;
  TestToggleButton& operator=(const TestToggleButton&) = delete;

  ~TestToggleButton() override {
    // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
    // Add/RemoveLayerFromRegions(). This is done so that the InkDrop doesn't
    // access the non-override versions in ~View.
    views::InkDrop::Remove(this);
  }

  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override {
    ++(*counter_);
    ToggleButton::AddLayerToRegion(layer, region);
  }

  void RemoveLayerFromRegions(ui::Layer* layer) override {
    --(*counter_);
    ToggleButton::RemoveLayerFromRegions(layer);
  }

  using View::Focus;

 private:
  const raw_ptr<int> counter_;
};

class ToggleButtonTest : public ViewsTestBase {
 public:
  ToggleButtonTest() = default;

  ToggleButtonTest(const ToggleButtonTest&) = delete;
  ToggleButtonTest& operator=(const ToggleButtonTest&) = delete;

  ~ToggleButtonTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a widget so that the ToggleButton can query the hover state
    // correctly.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();
    widget_->SetContentsView(std::make_unique<TestToggleButton>(&counter_));
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  int counter() const { return counter_; }
  Widget* widget() { return widget_.get(); }
  TestToggleButton* button() {
    return static_cast<TestToggleButton*>(widget_->GetContentsView());
  }

 private:
  std::unique_ptr<Widget> widget_;
  int counter_ = 0;
};

// Starts ink drop animation on a ToggleButton and destroys the button.
// The test verifies that the ink drop layer is removed properly when the
// ToggleButton gets destroyed.
TEST_F(ToggleButtonTest, ToggleButtonDestroyed) {
  gfx::Point center(10, 10);
  button()->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(1, counter());
  delete button();
  EXPECT_EQ(0, counter());
}

// Make sure nothing bad happens when the widget is destroyed while the
// ToggleButton has focus (and is showing a ripple).
TEST_F(ToggleButtonTest, ShutdownWithFocus) {
  button()->RequestFocus();
}

// Verify that ToggleButton::accepts_events_ works as expected.
TEST_F(ToggleButtonTest, AcceptEvents) {
  EXPECT_FALSE(button()->GetIsOn());
  ui::test::EventGenerator generator(GetRootWindow(widget()));
  generator.MoveMouseTo(widget()->GetClientAreaBoundsInScreen().CenterPoint());

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

TEST_F(ToggleButtonTest, AccessibleCheckedStateChange) {
  views::test::AXEventCounter ax_counter(views::AXEventManager::Get());
  ui::AXNodeData data;
  EXPECT_EQ(
      ax_counter.GetCount(ax::mojom::Event::kCheckedStateChanged, button()), 0);
  button()->SetIsOn(true);
  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(
      ax_counter.GetCount(ax::mojom::Event::kCheckedStateChanged, button()), 1);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kTrue);

  data = ui::AXNodeData();
  button()->SetIsOn(false);
  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);
  EXPECT_EQ(
      ax_counter.GetCount(ax::mojom::Event::kCheckedStateChanged, button()), 2);
}

}  // namespace views
