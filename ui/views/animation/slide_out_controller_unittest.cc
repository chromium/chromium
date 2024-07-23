// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/slide_out_controller.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/animation/slide_out_controller_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace views {

namespace {
constexpr int kSwipeControlWidth = 30;  // px
constexpr int kTargetWidth = 200;       // px
}  // namespace

class TestSlideOutControllerDelegate : public SlideOutControllerDelegate {
 public:
  explicit TestSlideOutControllerDelegate(View* target) : target_(target) {}
  ~TestSlideOutControllerDelegate() override = default;

  ui::Layer* GetSlideOutLayer() override { return target_->layer(); }

  void OnSlideStarted() override { ++slide_started_count_; }

  void OnSlideChanged(bool in_progress) override {
    slide_changed_last_value_ = in_progress;
    ++slide_changed_count_;
  }

  bool IsOnSlideChangedCalled() const { return (slide_changed_count_ > 0); }

  void OnSlideOut() override { ++slide_out_count_; }

  void reset() {
    slide_started_count_ = 0;
    slide_changed_count_ = 0;
    slide_out_count_ = 0;
    slide_changed_last_value_ = std::nullopt;
  }

  std::optional<bool> slide_changed_last_value_;
  int slide_started_count_ = 0;
  int slide_changed_count_ = 0;
  int slide_out_count_ = 0;

 private:
  const raw_ptr<View> target_;
};

class SlideOutControllerTest : public ViewsTestBase {
 public:
  SlideOutControllerTest() = default;
  ~SlideOutControllerTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = std::make_unique<Widget>();

    Widget::InitParams params = CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(50, 50, 650, 650);
    widget_->Init(std::move(params));
    View* root = widget_->GetRootView();

    View* target_ = new View();
    target_->SetPaintToLayer(ui::LAYER_TEXTURED);
    target_->SetSize(gfx::Size(kTargetWidth, 50));

    root->AddChildView(target_);
    widget_->Show();

    delegate_ = std::make_unique<TestSlideOutControllerDelegate>(target_);
    slide_out_controller_ =
        std::make_unique<SlideOutController>(target_, delegate_.get());
  }

  void TearDown() override {
    slide_out_controller_.reset();
    delegate_.reset();
    widget_.reset();

    ViewsTestBase::TearDown();
  }

 protected:
  SlideOutController* slide_out_controller() {
    return slide_out_controller_.get();
  }

  TestSlideOutControllerDelegate* delegate() { return delegate_.get(); }

  void PostSequentialGestureEvent(const ui::GestureEventDetails& details) {
    // Set the timestamp ahead one microsecond.
    sequential_event_timestamp_ += base::Microseconds(1);

    ui::GestureEvent gesture_event(
        0, 0, ui::EF_NONE, base::TimeTicks() + sequential_event_timestamp_,
        details);
    slide_out_controller()->OnGestureEvent(&gesture_event);
  }

  void PostSequentialSwipeEvent(int swipe_amount) {
    PostSequentialGestureEvent(
        ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));
    PostSequentialGestureEvent(ui::GestureEventDetails(
        ui::EventType::kGestureScrollUpdate, swipe_amount, 0));
    PostSequentialGestureEvent(
        ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));
  }

  void PostTrackPadSwipeEvent(ui::EventType type,
                              int swipe_amount,
                              int finger_count) {
    auto scroll_event =
        ui::ScrollEvent(type, gfx::PointF(), gfx::PointF(), base::TimeTicks(),
                        0, swipe_amount, 0, 0, 0, finger_count);
    slide_out_controller()->OnScrollEvent(&scroll_event);
  }

 private:
  std::unique_ptr<Widget> widget_;
  std::unique_ptr<SlideOutController> slide_out_controller_;
  std::unique_ptr<TestSlideOutControllerDelegate> delegate_;
  base::TimeDelta sequential_event_timestamp_;
};

TEST_F(SlideOutControllerTest, OnGestureEventAndDelegate) {
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_FALSE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
}

TEST_F(SlideOutControllerTest, SlideOutAndClose) {
  // Place a finger on notification.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_EQ(0, delegate()->slide_changed_count_);
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Move the finger horizontally by 101 px. (101 px is more than half of the
  // target width 200 px)
  PostSequentialGestureEvent(ui::GestureEventDetails(
      ui::EventType::kGestureScrollUpdate, kTargetWidth / 2 + 1, 0));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Release the finger.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_FALSE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  // The target has been scrolled out and the current location is moved by the
  // width (200px).
  EXPECT_EQ(kTargetWidth,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Ensure a deferred OnSlideOut handler is called.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(1, delegate()->slide_out_count_);
}

TEST_F(SlideOutControllerTest, SlideLittleAmountAndNotClose) {
  // Place a finger on notification.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Move the finger horizontally by 99 px. (99 px is less than half of the
  // target width 200 px)
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, 99, 0));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Release the finger.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_FALSE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  // The target has been moved back to the origin.
  EXPECT_EQ(0.f,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Ensure no deferred SlideOut handler.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);
}

class SwipeControlTest
    : public SlideOutControllerTest,
      public testing::WithParamInterface<SlideOutController::SlideMode> {
 public:
  SwipeControlTest() = default;

  SwipeControlTest(const SwipeControlTest&) = delete;
  SwipeControlTest& operator=(const SwipeControlTest&) = delete;

  ~SwipeControlTest() override = default;

  void SetUp() override {
    SlideOutControllerTest::SetUp();
    slide_out_controller()->set_slide_mode(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SwipeControlTest,
    ::testing::Values(SlideOutController::SlideMode::kFull,
                      SlideOutController::SlideMode::kPartial));

TEST_P(SwipeControlTest, SetSwipeControlWidth_SwipeLessThanControlWidth) {
  // Set the width of swipe control.
  slide_out_controller()->SetSwipeControlWidth(kSwipeControlWidth);

  // Place a finger on notification.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Move the finger horizontally by 29 px. (29 px is less than the swipe
  // control width).
  PostSequentialGestureEvent(ui::GestureEventDetails(
      ui::EventType::kGestureScrollUpdate, kSwipeControlWidth - 1, 0));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Release the finger.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_FALSE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  // The target has been moved back to the origin.
  EXPECT_EQ(0.f,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Ensure no deferred SlideOut handler.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);
}

TEST_P(SwipeControlTest, SwipeControlWidth_SwipeMoreThanControlWidth) {
  // Set the width of swipe control.
  slide_out_controller()->SetSwipeControlWidth(kSwipeControlWidth);

  // Place a finger on notification.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Move the finger horizontally by 31 px. (31 px is more than the swipe
  // control width).
  PostSequentialGestureEvent(ui::GestureEventDetails(
      ui::EventType::kGestureScrollUpdate, kSwipeControlWidth + 1, 0));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Release the finger.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  // Slide is in progress.
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  // Swipe amount is the swipe control width.
  EXPECT_EQ(kSwipeControlWidth,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Ensure no deferred SlideOut handler.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);
}

TEST_P(SwipeControlTest, SetSwipeControlWidth_SwipeOut) {
  const bool swipe_out_supported =
      slide_out_controller()->mode() == SlideOutController::SlideMode::kFull;

  // Set the width of swipe control.
  slide_out_controller()->SetSwipeControlWidth(kSwipeControlWidth);

  // Place a finger on notification.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Move the finger horizontally by 101 px. (101 px is more than the half of
  // the target width).
  PostSequentialGestureEvent(ui::GestureEventDetails(
      ui::EventType::kGestureScrollUpdate, kTargetWidth / 2 + 1, 0));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Release the finger.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));

  // ... and it is automatically slided out if |swipe_out_supported|.
  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(delegate()->slide_changed_last_value_.value(),
            !swipe_out_supported);
  EXPECT_EQ(0, delegate()->slide_out_count_);
  EXPECT_EQ(swipe_out_supported ? kTargetWidth : kSwipeControlWidth,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Ensure a deferred SlideOut handler is called once.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(swipe_out_supported ? 1 : 0, delegate()->slide_out_count_);
}

TEST_P(SwipeControlTest, SwipeControlWidth_SnapAndSwipeOut) {
  const bool swipe_out_supported =
      slide_out_controller()->mode() == SlideOutController::SlideMode::kFull;

  // Set the width of swipe control.
  slide_out_controller()->SetSwipeControlWidth(kSwipeControlWidth);

  // Snap to the swipe control.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));
  PostSequentialGestureEvent(ui::GestureEventDetails(
      ui::EventType::kGestureScrollUpdate, kSwipeControlWidth, 0));
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));
  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  EXPECT_EQ(kSwipeControlWidth,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Swipe horizontally by 70 px.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, 70, 0));
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));

  // ... and it is automatically slided out if if |swipe_out_supported|.
  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(delegate()->slide_changed_last_value_.value(),
            !swipe_out_supported);
  EXPECT_EQ(0, delegate()->slide_out_count_);
  EXPECT_EQ(swipe_out_supported ? kTargetWidth : kSwipeControlWidth,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Ensure a deferred OnSlideOut handler is called.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(swipe_out_supported ? 1 : 0, delegate()->slide_out_count_);
}

TEST_P(SwipeControlTest, SwipeControlWidth_SnapAndSnapToControl) {
  // Set the width of swipe control.
  slide_out_controller()->SetSwipeControlWidth(kSwipeControlWidth);

  // Snap to the swipe control.
  PostSequentialSwipeEvent(kSwipeControlWidth + 10);
  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  EXPECT_EQ(kSwipeControlWidth,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Swipe horizontally by 40 px for the same direction.
  PostSequentialSwipeEvent(40);

  // Snap automatically back to the swipe control.
  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  EXPECT_EQ(kSwipeControlWidth,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Ensure no deferred OnSlideOut handler.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);
}

TEST_P(SwipeControlTest, SwipeControlWidth_SnapAndBackToOrigin) {
  // Set the width of swipe control.
  slide_out_controller()->SetSwipeControlWidth(kSwipeControlWidth);

  // Snap to the swipe control.
  PostSequentialSwipeEvent(kSwipeControlWidth + 20);
  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  EXPECT_EQ(kSwipeControlWidth,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Swipe to the reversed direction by -1 px.
  PostSequentialSwipeEvent(-1);

  // Snap automatically back to the origin.
  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_FALSE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  EXPECT_EQ(0,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Ensure no deferred OnSlideOut handler.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);
}

TEST_P(SwipeControlTest, SwipeControlWidth_NotSnapAndBackToOrigin) {
  // Set the width of swipe control.
  slide_out_controller()->SetSwipeControlWidth(kSwipeControlWidth);

  // Swipe partially but it's not enough to snap to the swipe control. So it is
  // back to the origin
  PostSequentialSwipeEvent(kSwipeControlWidth - 1);
  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_FALSE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  EXPECT_EQ(0,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Ensure no deferred OnSlideOut handler.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);
}

// Test class for making sure trackpad gestures work to trigger slide out
// events.
class TrackPadGestureTest : public SlideOutControllerTest {
 public:
  TrackPadGestureTest() = default;

  explicit TrackPadGestureTest(const SwipeControlTest&) = delete;
  TrackPadGestureTest& operator=(const TrackPadGestureTest&) = delete;

  ~TrackPadGestureTest() override = default;

  void SetUp() override {
    SlideOutControllerTest::SetUp();
    slide_out_controller()->set_trackpad_gestures_enabled(true);
  }
};

TEST_F(TrackPadGestureTest, SlideOut) {
  int width = delegate()->GetSlideOutLayer()->bounds().width();
  // A slide out should not be triggered if the scroll offset isn't less greater
  // than the view's width.
  PostTrackPadSwipeEvent(ui::EventType::kScroll, width,
                         /*finger_count=*/2);
  PostTrackPadSwipeEvent(ui::EventType::kScrollFlingStart, 0, 2);
  EXPECT_EQ(0, delegate()->slide_out_count_);

  // A slide out should not be triggered if the finger count is not equal to 2.
  PostTrackPadSwipeEvent(ui::EventType::kScroll, width + 1,
                         /*finger_count=*/3);
  PostTrackPadSwipeEvent(ui::EventType::kScrollFlingStart, 0,
                         /*finger_count=*/3);
  EXPECT_EQ(0, delegate()->slide_out_count_);

  PostTrackPadSwipeEvent(ui::EventType::kScroll, width + 1,
                         /*finger_count=*/2);
  // A slide out should not be triggered until the
  // `EventType::kScrollFlingStart` is posted.
  EXPECT_EQ(0, delegate()->slide_out_count_);

  PostTrackPadSwipeEvent(ui::EventType::kScrollFlingStart, 0, 2);
  EXPECT_EQ(1, delegate()->slide_out_count_);
}

}  // namespace views
