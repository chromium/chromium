// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/slider.h"

#include <memory>
#include <string>

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/events/event.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/test/slider_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_utils.h"

namespace {

// A views::SliderListener that tracks simple event call history.
class TestSliderListener : public views::SliderListener {
 public:
  TestSliderListener();
  ~TestSliderListener() override;

  int last_event_epoch() {
    return last_event_epoch_;
  }

  int last_drag_started_epoch() {
    return last_drag_started_epoch_;
  }

  int last_drag_ended_epoch() {
    return last_drag_ended_epoch_;
  }

  views::Slider* last_drag_started_sender() {
    return last_drag_started_sender_;
  }

  views::Slider* last_drag_ended_sender() {
    return last_drag_ended_sender_;
  }

  // Resets the state of this as if it were newly created.
  virtual void ResetCallHistory();

  // views::SliderListener:
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;
  void SliderDragStarted(views::Slider* sender) override;
  void SliderDragEnded(views::Slider* sender) override;

 private:
  // The epoch of the last event.
  int last_event_epoch_ = 0;
  // The epoch of the last time SliderDragStarted was called.
  int last_drag_started_epoch_ = -1;
  // The epoch of the last time SliderDragEnded was called.
  int last_drag_ended_epoch_ = -1;
  // The sender from the last SliderDragStarted call.
  views::Slider* last_drag_started_sender_ = nullptr;
  // The sender from the last SliderDragEnded call.
  views::Slider* last_drag_ended_sender_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestSliderListener);
};

TestSliderListener::TestSliderListener() = default;

TestSliderListener::~TestSliderListener() {
  last_drag_started_sender_ = nullptr;
  last_drag_ended_sender_ = nullptr;
}

void TestSliderListener::ResetCallHistory() {
  last_event_epoch_ = 0;
  last_drag_started_epoch_ = -1;
  last_drag_ended_epoch_ = -1;
  last_drag_started_sender_ = nullptr;
  last_drag_ended_sender_ = nullptr;
}

void TestSliderListener::SliderValueChanged(views::Slider* sender,
                                            float value,
                                            float old_value,
                                            views::SliderChangeReason reason) {
  ++last_event_epoch_;
}

void TestSliderListener::SliderDragStarted(views::Slider* sender) {
  last_drag_started_sender_ = sender;
  last_drag_started_epoch_ = ++last_event_epoch_;
}

void TestSliderListener::SliderDragEnded(views::Slider* sender) {
  last_drag_ended_sender_ = sender;
  last_drag_ended_epoch_ = ++last_event_epoch_;
}

class TestAXEventObserver : public views::AXEventObserver {
 public:
  TestAXEventObserver() { views::AXEventManager::Get()->AddObserver(this); }

  ~TestAXEventObserver() override {
    views::AXEventManager::Get()->RemoveObserver(this);
  }

  bool value_changed() const { return value_changed_; }

  // views::AXEventObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override {
    if (event_type == ax::mojom::Event::kValueChanged)
      value_changed_ = true;
  }

 private:
  bool value_changed_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestAXEventObserver);
};

}  // namespace

namespace views {

// Base test fixture for Slider tests.
class SliderTest : public views::ViewsTestBase {
 public:
  SliderTest() = default;
  ~SliderTest() override = default;

 protected:
  Slider* slider() {
    return slider_;
  }

  TestSliderListener& slider_listener() {
    return slider_listener_;
  }

  int max_x() {
    return max_x_;
  }

  int max_y() {
    return max_y_;
  }

  virtual void ClickAt(int x, int y);

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  ui::test::EventGenerator* event_generator() {
    return event_generator_.get();
  }

 private:
  // The Slider to be tested.
  Slider* slider_ = nullptr;
  // A simple SliderListener test double.
  TestSliderListener slider_listener_;
  // Stores the default locale at test setup so it can be restored
  // during test teardown.
  std::string default_locale_;
  // The maximum x value within the bounds of the slider.
  int max_x_ = 0;
  // The maximum y value within the bounds of the slider.
  int max_y_ = 0;
  // The widget container for the slider being tested.
  views::Widget* widget_ = nullptr;
  // An event generator.
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  DISALLOW_COPY_AND_ASSIGN(SliderTest);
};

void SliderTest::SetUp() {
  views::ViewsTestBase::SetUp();

  slider_ = new Slider(nullptr);
  View* view = slider_;
  gfx::Size size = view->GetPreferredSize();
  view->SetSize(size);
  max_x_ = size.width() - 1;
  max_y_ = size.height() - 1;
  default_locale_ = base::i18n::GetConfiguredLocale();

  views::Widget::InitParams init_params(CreateParams(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS));
  init_params.bounds = gfx::Rect(size);

  widget_ = new views::Widget();
  widget_->Init(std::move(init_params));
  widget_->SetContentsView(slider_);
  widget_->Show();

  event_generator_ =
      std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_));
}

void SliderTest::TearDown() {
  if (widget_ && !widget_->IsClosed())
    widget_->Close();

  base::i18n::SetICUDefaultLocale(default_locale_);

  views::ViewsTestBase::TearDown();
}

void SliderTest::ClickAt(int x, int y) {
  gfx::Point point(x, y);
  event_generator_->MoveMouseTo(point);
  event_generator_->ClickLeftButton();
}

TEST_F(SliderTest, UpdateFromClickHorizontal) {
  ClickAt(0, 0);
  EXPECT_EQ(0.0f, slider()->GetValue());

  ClickAt(max_x(), 0);
  EXPECT_EQ(1.0f, slider()->GetValue());
}

TEST_F(SliderTest, UpdateFromClickRTLHorizontal) {
  base::i18n::SetICUDefaultLocale("he");

  ClickAt(0, 0);
  EXPECT_EQ(1.0f, slider()->GetValue());

  ClickAt(max_x(), 0);
  EXPECT_EQ(0.0f, slider()->GetValue());
}

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !defined(OS_MACOSX) || defined(USE_AURA)

// Test the slider location after a tap gesture.
TEST_F(SliderTest, SliderValueForTapGesture) {
  // Tap below the minimum.
  slider()->SetValue(0.5);
  event_generator()->GestureTapAt(gfx::Point(0, 0));
  EXPECT_FLOAT_EQ(0, slider()->GetValue());

  // Tap above the maximum.
  slider()->SetValue(0.5);
  event_generator()->GestureTapAt(gfx::Point(max_x(), max_y()));
  EXPECT_FLOAT_EQ(1, slider()->GetValue());

  // Tap somwhere in the middle.
  slider()->SetValue(0.5);
  event_generator()->GestureTapAt(gfx::Point(0.75 * max_x(), 0.75 * max_y()));
  EXPECT_NEAR(0.75, slider()->GetValue(), 0.03);
}

// Test the slider location after a scroll gesture.
TEST_F(SliderTest, SliderValueForScrollGesture) {
  // Scroll below the minimum.
  slider()->SetValue(0.5);
  event_generator()->GestureScrollSequence(
      gfx::Point(0.5 * max_x(), 0.5 * max_y()), gfx::Point(0, 0),
      base::TimeDelta::FromMilliseconds(10), 5 /* steps */);
  EXPECT_EQ(0, slider()->GetValue());

  // Scroll above the maximum.
  slider()->SetValue(0.5);
  event_generator()->GestureScrollSequence(
      gfx::Point(0.5 * max_x(), 0.5 * max_y()), gfx::Point(max_x(), max_y()),
      base::TimeDelta::FromMilliseconds(10), 5 /* steps */);
  EXPECT_EQ(1, slider()->GetValue());

  // Scroll somewhere in the middle.
  slider()->SetValue(0.25);
  event_generator()->GestureScrollSequence(
      gfx::Point(0.25 * max_x(), 0.25 * max_y()),
      gfx::Point(0.75 * max_x(), 0.75 * max_y()),
      base::TimeDelta::FromMilliseconds(10), 5 /* steps */);
  EXPECT_NEAR(0.75, slider()->GetValue(), 0.03);
}

// Test the slider location by adjusting it using keyboard.
TEST_F(SliderTest, SliderValueForKeyboard) {
  float value = 0.5;
  slider()->SetValue(value);
  slider()->RequestFocus();
  event_generator()->PressKey(ui::VKEY_RIGHT, 0);
  EXPECT_GT(slider()->GetValue(), value);

  slider()->SetValue(value);
  event_generator()->PressKey(ui::VKEY_LEFT, 0);
  EXPECT_LT(slider()->GetValue(), value);

  slider()->SetValue(value);
  event_generator()->PressKey(ui::VKEY_UP, 0);
  EXPECT_GT(slider()->GetValue(), value);

  slider()->SetValue(value);
  event_generator()->PressKey(ui::VKEY_DOWN, 0);
  EXPECT_LT(slider()->GetValue(), value);

  // RTL reverse left/right but not up/down.
  base::i18n::SetICUDefaultLocale("he");
  EXPECT_TRUE(base::i18n::IsRTL());

  event_generator()->PressKey(ui::VKEY_RIGHT, 0);
  EXPECT_LT(slider()->GetValue(), value);

  slider()->SetValue(value);
  event_generator()->PressKey(ui::VKEY_LEFT, 0);
  EXPECT_GT(slider()->GetValue(), value);

  slider()->SetValue(value);
  event_generator()->PressKey(ui::VKEY_UP, 0);
  EXPECT_GT(slider()->GetValue(), value);

  slider()->SetValue(value);
  event_generator()->PressKey(ui::VKEY_DOWN, 0);
  EXPECT_LT(slider()->GetValue(), value);
}

// Verifies the correct SliderListener events are raised for a tap gesture.
TEST_F(SliderTest, SliderListenerEventsForTapGesture) {
  test::SliderTestApi slider_test_api(slider());
  slider_test_api.SetListener(&slider_listener());

  event_generator()->GestureTapAt(gfx::Point(0, 0));
  EXPECT_EQ(1, slider_listener().last_drag_started_epoch());
  EXPECT_EQ(2, slider_listener().last_drag_ended_epoch());
  EXPECT_EQ(slider(), slider_listener().last_drag_started_sender());
  EXPECT_EQ(slider(), slider_listener().last_drag_ended_sender());
}

// Verifies the correct SliderListener events are raised for a scroll gesture.
TEST_F(SliderTest, SliderListenerEventsForScrollGesture) {
  test::SliderTestApi slider_test_api(slider());
  slider_test_api.SetListener(&slider_listener());

  event_generator()->GestureScrollSequence(
    gfx::Point(0.25 * max_x(), 0.25 * max_y()),
    gfx::Point(0.75 * max_x(), 0.75 * max_y()),
    base::TimeDelta::FromMilliseconds(0),
    5 /* steps */);

  EXPECT_EQ(1, slider_listener().last_drag_started_epoch());
  EXPECT_GT(slider_listener().last_drag_ended_epoch(),
            slider_listener().last_drag_started_epoch());
  EXPECT_EQ(slider(), slider_listener().last_drag_started_sender());
  EXPECT_EQ(slider(), slider_listener().last_drag_ended_sender());
}

// Verifies the correct SliderListener events are raised for a multi
// finger scroll gesture.
TEST_F(SliderTest, SliderListenerEventsForMultiFingerScrollGesture) {
  test::SliderTestApi slider_test_api(slider());
  slider_test_api.SetListener(&slider_listener());

  gfx::Point points[] = {gfx::Point(0, 0.1 * max_y()),
                         gfx::Point(0, 0.2 * max_y())};
  event_generator()->GestureMultiFingerScroll(2 /* count */, points,
      0 /* event_separation_time_ms */, 5 /* steps */,
      2 /* move_x */, 0 /* move_y */);

  EXPECT_EQ(1, slider_listener().last_drag_started_epoch());
  EXPECT_GT(slider_listener().last_drag_ended_epoch(),
            slider_listener().last_drag_started_epoch());
  EXPECT_EQ(slider(), slider_listener().last_drag_started_sender());
  EXPECT_EQ(slider(), slider_listener().last_drag_ended_sender());
}

// Verifies the correct SliderListener events are raised for an accessible
// slider.
TEST_F(SliderTest, SliderRaisesA11yEvents) {
  TestAXEventObserver observer;
  EXPECT_FALSE(observer.value_changed());

  // First, detach/reattach the slider without setting value.
  // Temporarily detach the slider.
  View* root_view = slider()->parent();
  root_view->RemoveChildView(slider());

  // Re-attachment should cause nothing to get fired.
  root_view->AddChildView(slider());
  EXPECT_FALSE(observer.value_changed());

  // Now, set value before reattaching.
  root_view->RemoveChildView(slider());

  // Value changes won't trigger accessibility events before re-attachment.
  slider()->SetValue(22);
  EXPECT_FALSE(observer.value_changed());

  // Re-attachment should trigger the value change.
  root_view->AddChildView(slider());
  EXPECT_TRUE(observer.value_changed());
}

#endif  // !defined(OS_MACOSX) || defined(USE_AURA)

}  // namespace views
