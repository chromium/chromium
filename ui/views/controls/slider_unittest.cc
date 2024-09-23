// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/slider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
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
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/ax_event_counter.h"
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
  TestSliderListener() = default;

  TestSliderListener(const TestSliderListener&) = delete;
  TestSliderListener& operator=(const TestSliderListener&) = delete;

  ~TestSliderListener() override = default;

  int last_event_epoch() { return last_event_epoch_; }

  int last_drag_started_epoch() { return last_drag_started_epoch_; }

  int last_drag_ended_epoch() { return last_drag_ended_epoch_; }

  views::Slider* last_drag_started_sender() {
    return last_drag_started_sender_;
  }

  views::Slider* last_drag_ended_sender() { return last_drag_ended_sender_; }

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
  raw_ptr<views::Slider> last_drag_started_sender_ = nullptr;
  // The sender from the last SliderDragEnded call.
  raw_ptr<views::Slider> last_drag_ended_sender_ = nullptr;
};

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

}  // namespace

namespace views {

// Base test fixture for Slider tests.
enum class TestSliderType {
  kContinuousTest,       // ContinuousSlider
  kDiscreteEnd2EndTest,  // DiscreteSlider with 0 and 1 in the list of values.
  kDiscreteInnerTest,    // DiscreteSlider excluding 0 and 1.
};

// Parameter specifies whether to test ContinuousSlider (true) or
// DiscreteSlider(false).
class SliderTest : public views::ViewsTestBase,
                   public testing::WithParamInterface<TestSliderType> {
 public:
  SliderTest() = default;

  SliderTest(const SliderTest&) = delete;
  SliderTest& operator=(const SliderTest&) = delete;

  ~SliderTest() override = default;

 protected:
  Slider* slider() { return static_cast<Slider*>(widget_->GetContentsView()); }

  int max_x() { return max_x_; }

  int max_y() { return max_y_; }

  virtual void ClickAt(int x, int y);

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

  const base::flat_set<float>& values() const { return values_; }

  // Returns minimum and maximum possible slider values with respect to test
  // param.
  float GetMinValue() const;
  float GetMaxValue() const;

 private:
  // Populated values for discrete slider.
  base::flat_set<float> values_;
  // Stores the default locale at test setup so it can be restored
  // during test teardown.
  std::string default_locale_;
  // The maximum x value within the bounds of the slider.
  int max_x_ = 0;
  // The maximum y value within the bounds of the slider.
  int max_y_ = 0;
  // The widget container for the slider being tested.
  std::unique_ptr<Widget> widget_;
  // An event generator.
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

void SliderTest::ClickAt(int x, int y) {
  gfx::Point point =
      slider()->GetBoundsInScreen().origin() + gfx::Vector2d(x, y);
  event_generator_->MoveMouseTo(point);
  event_generator_->ClickLeftButton();
}

void SliderTest::SetUp() {
  views::ViewsTestBase::SetUp();

  auto slider = std::make_unique<Slider>();
  switch (GetParam()) {
    case TestSliderType::kContinuousTest:
      break;
    case TestSliderType::kDiscreteEnd2EndTest:
      values_ = {0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1};
      slider->SetAllowedValues(&values_);
      break;
    case TestSliderType::kDiscreteInnerTest:
      values_ = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9};
      slider->SetAllowedValues(&values_);
      break;
    default:
      NOTREACHED();
  }
  gfx::Size size = slider->GetPreferredSize({});
  slider->SetSize(size);
  max_x_ = size.width() - 1;
  max_y_ = size.height() - 1;
  default_locale_ = base::i18n::GetConfiguredLocale();

  views::Widget::InitParams init_params(
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS));
  init_params.bounds = gfx::Rect(size);

  widget_ = std::make_unique<Widget>();
  widget_->Init(std::move(init_params));
  widget_->SetContentsView(std::move(slider));
  widget_->Show();

  event_generator_ =
      std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_.get()));
}

void SliderTest::TearDown() {
  widget_.reset();
  base::i18n::SetICUDefaultLocale(default_locale_);

  views::ViewsTestBase::TearDown();
}

float SliderTest::GetMinValue() const {
  if (GetParam() == TestSliderType::kContinuousTest)
    return 0.0f;

  return *values().cbegin();
}

float SliderTest::GetMaxValue() const {
  if (GetParam() == TestSliderType::kContinuousTest)
    return 1.0f;

  return *values().crbegin();
}

TEST_P(SliderTest, UpdateFromClickHorizontal) {
  ClickAt(0, 0);
  EXPECT_EQ(GetMinValue(), slider()->GetValue());

  ClickAt(max_x(), 0);
  EXPECT_EQ(GetMaxValue(), slider()->GetValue());
}

TEST_P(SliderTest, UpdateFromClickRTLHorizontal) {
  base::i18n::SetICUDefaultLocale("he");

  ClickAt(0, 0);
  EXPECT_EQ(GetMaxValue(), slider()->GetValue());

  ClickAt(max_x(), 0);
  EXPECT_EQ(GetMinValue(), slider()->GetValue());
}

TEST_P(SliderTest, NukeAllowedValues) {
  //  No more Allowed Values.
  slider()->SetAllowedValues(nullptr);
  // Verify that slider is now able to take full scale despite the original
  // configuration.
  ClickAt(0, 0);
  EXPECT_EQ(0, slider()->GetValue());

  ClickAt(max_x(), 0);
  EXPECT_EQ(1, slider()->GetValue());

  const int position = max_x() / 18.0f;
  ClickAt(position, 0);

  // These values were copied from the slider source.
  constexpr float kThumbRadius = 4.f;
  constexpr float kThumbWidth = 2 * kThumbRadius;
  // This formula is copied here from Slider::MoveButtonTo() to verify that
  // slider does use full scale and previous Allowed Values no longer affect
  // calculations.
  EXPECT_FLOAT_EQ(
      static_cast<float>(
          position - test::SliderTestApi(slider()).initial_button_offset()) /
          (slider()->width() - kThumbWidth),
      slider()->GetValue());
}

TEST_P(SliderTest, AccessibleRole) {
  ui::AXNodeData data;
  slider()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kSlider);

  slider()->GetViewAccessibility().SetRole(ax::mojom::Role::kMeter);

  data = ui::AXNodeData();
  slider()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kMeter);
}

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !BUILDFLAG(IS_MAC) || defined(USE_AURA)

// Test the slider location after a tap gesture.
TEST_P(SliderTest, SliderValueForTapGesture) {
  // Tap below the minimum.
  slider()->SetValue(0.5);
  event_generator()->GestureTapAt(gfx::Point(0, 0));
  EXPECT_FLOAT_EQ(GetMinValue(), slider()->GetValue());

  // Tap above the maximum.
  slider()->SetValue(0.5);
  event_generator()->GestureTapAt(gfx::Point(max_x(), max_y()));
  EXPECT_FLOAT_EQ(GetMaxValue(), slider()->GetValue());

  // Tap somewhere in the middle.
  // 0.76 is closer to 0.8 which is important for discrete slider.
  slider()->SetValue(0.5);
  event_generator()->GestureTapAt(gfx::Point(0.76 * max_x(), 0.76 * max_y()));
  if (GetParam() == TestSliderType::kContinuousTest) {
    EXPECT_NEAR(0.76, slider()->GetValue(), 0.03);
  } else {
    // Discrete slider has 0.1 steps.
    EXPECT_NEAR(0.8, slider()->GetValue(), 0.01);
  }
}

// Test the slider location after a scroll gesture.
TEST_P(SliderTest, SliderValueForScrollGesture) {
  // Scroll below the minimum.
  slider()->SetValue(0.5);
  event_generator()->GestureScrollSequence(
      gfx::Point(0.5 * max_x(), 0.5 * max_y()), gfx::Point(0, 0),
      base::Milliseconds(10), 5 /* steps */);
  EXPECT_EQ(GetMinValue(), slider()->GetValue());

  // Scroll above the maximum.
  slider()->SetValue(0.5);
  event_generator()->GestureScrollSequence(
      gfx::Point(0.5 * max_x(), 0.5 * max_y()), gfx::Point(max_x(), max_y()),
      base::Milliseconds(10), 5 /* steps */);
  EXPECT_EQ(GetMaxValue(), slider()->GetValue());

  // Scroll somewhere in the middle.
  // 0.78 is closer to 0.8 which is important for discrete slider.
  slider()->SetValue(0.25);
  event_generator()->GestureScrollSequence(
      gfx::Point(0.25 * max_x(), 0.25 * max_y()),
      gfx::Point(0.78 * max_x(), 0.78 * max_y()), base::Milliseconds(10),
      5 /* steps */);
  if (GetParam() == TestSliderType::kContinuousTest) {
    // Continuous slider.
    EXPECT_NEAR(0.78, slider()->GetValue(), 0.03);
  } else {
    // Discrete slider has 0.1 steps.
    EXPECT_NEAR(0.8, slider()->GetValue(), 0.01);
  }
}

// Test the slider location by adjusting it using keyboard.
TEST_P(SliderTest, SliderValueForKeyboard) {
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
TEST_P(SliderTest, SliderListenerEventsForTapGesture) {
  TestSliderListener slider_listener;
  test::SliderTestApi(slider()).SetListener(&slider_listener);

  event_generator()->GestureTapAt(gfx::Point(0, 0));
  EXPECT_EQ(1, slider_listener.last_drag_started_epoch());
  EXPECT_EQ(2, slider_listener.last_drag_ended_epoch());
  EXPECT_EQ(slider(), slider_listener.last_drag_started_sender());
  EXPECT_EQ(slider(), slider_listener.last_drag_ended_sender());
  test::SliderTestApi(slider()).SetListener(nullptr);
}

// Verifies the correct SliderListener events are raised for a scroll gesture.
TEST_P(SliderTest, SliderListenerEventsForScrollGesture) {
  TestSliderListener slider_listener;
  test::SliderTestApi(slider()).SetListener(&slider_listener);

  event_generator()->GestureScrollSequence(
      gfx::Point(0.25 * max_x(), 0.25 * max_y()),
      gfx::Point(0.75 * max_x(), 0.75 * max_y()), base::Milliseconds(0),
      5 /* steps */);

  EXPECT_EQ(1, slider_listener.last_drag_started_epoch());
  EXPECT_GT(slider_listener.last_drag_ended_epoch(),
            slider_listener.last_drag_started_epoch());
  EXPECT_EQ(slider(), slider_listener.last_drag_started_sender());
  EXPECT_EQ(slider(), slider_listener.last_drag_ended_sender());
  test::SliderTestApi(slider()).SetListener(nullptr);
}

// Verifies the correct SliderListener events are raised for a multi
// finger scroll gesture.
TEST_P(SliderTest, SliderListenerEventsForMultiFingerScrollGesture) {
  TestSliderListener slider_listener;
  test::SliderTestApi(slider()).SetListener(&slider_listener);

  gfx::Point points[] = {gfx::Point(0, 0.1 * max_y()),
                         gfx::Point(0, 0.2 * max_y())};
  event_generator()->GestureMultiFingerScroll(
      2 /* count */, points, 0 /* event_separation_time_ms */, 5 /* steps */,
      2 /* move_x */, 0 /* move_y */);

  EXPECT_EQ(1, slider_listener.last_drag_started_epoch());
  EXPECT_GT(slider_listener.last_drag_ended_epoch(),
            slider_listener.last_drag_started_epoch());
  EXPECT_EQ(slider(), slider_listener.last_drag_started_sender());
  EXPECT_EQ(slider(), slider_listener.last_drag_ended_sender());
  test::SliderTestApi(slider()).SetListener(nullptr);
}

// Verifies the correct SliderListener events are raised for an accessible
// slider.
TEST_P(SliderTest, SliderRaisesA11yEvents) {
  test::AXEventCounter ax_counter(views::AXEventManager::Get());
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kValueChanged));

  // First, detach/reattach the slider without setting value.
  // Temporarily detach the slider.
  View* root_view = slider()->parent();
  auto owning_slider = root_view->RemoveChildViewT(slider());

  // Re-attachment should cause nothing to get fired.
  root_view->AddChildView(std::move(owning_slider));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kValueChanged));

  // Now, set value before reattaching.
  owning_slider = root_view->RemoveChildViewT(slider());

  // Value changes won't trigger accessibility events before re-attachment.
  owning_slider->SetValue(22);
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kValueChanged));

  // Re-attachment should trigger the value change.
  root_view->AddChildView(std::move(owning_slider));
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kValueChanged));
}

#endif  // !BUILDFLAG(IS_MAC) || defined(USE_AURA)

INSTANTIATE_TEST_SUITE_P(All,
                         SliderTest,
                         ::testing::Values(TestSliderType::kContinuousTest,
                                           TestSliderType::kDiscreteEnd2EndTest,
                                           TestSliderType::kDiscreteInnerTest));
}  // namespace views
