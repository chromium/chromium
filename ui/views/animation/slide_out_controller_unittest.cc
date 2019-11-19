// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/slide_out_controller.h"

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
    slide_changed_last_value_ = base::nullopt;
  }

  base::Optional<bool> slide_changed_last_value_;
  int slide_started_count_ = 0;
  int slide_changed_count_ = 0;
  int slide_out_count_ = 0;

 private:
  View* const target_;
};

class SlideOutControllerTest : public ViewsTestBase {
 public:
  SlideOutControllerTest() = default;
  ~SlideOutControllerTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = std::make_unique<Widget>();

    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
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
    sequential_event_timestamp_ += base::TimeDelta::FromMicroseconds(1);

    ui::GestureEvent gesture_event(
        0, 0, ui::EF_NONE, base::TimeTicks() + sequential_event_timestamp_,
        details);
    slide_out_controller()->OnGestureEvent(&gesture_event);
  }

  void PostSequentialSwipeEvent(int swipe_amount) {
    PostSequentialGestureEvent(
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
    PostSequentialGestureEvent(
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, swipe_amount, 0));
    PostSequentialGestureEvent(
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
  }

 private:
  std::unique_ptr<Widget> widget_;
  std::unique_ptr<SlideOutController> slide_out_controller_;
  std::unique_ptr<TestSlideOutControllerDelegate> delegate_;
  base::TimeDelta sequential_event_timestamp_;
};

TEST_F(SlideOutControllerTest, OnGestureEventAndDelegate) {
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_FALSE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
}

TEST_F(SlideOutControllerTest, SlideOutAndClose) {
  // Place a finger on notification.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_EQ(0, delegate()->slide_changed_count_);
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Move the finger horizontally by 101 px. (101 px is more than half of the
  // target width 200 px)
  PostSequentialGestureEvent(ui::GestureEventDetails(
      ui::ET_GESTURE_SCROLL_UPDATE, kTargetWidth / 2 + 1, 0));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Release the finger.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));

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
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Move the finger horizontally by 99 px. (99 px is less than half of the
  // target width 200 px)
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 99, 0));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Release the finger.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));

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

TEST_F(SlideOutControllerTest, SetSwipeControlWidth_SwipeLessThanControlWidth) {
  // Set the width of swipe control.
  slide_out_controller()->SetSwipeControlWidth(kSwipeControlWidth);

  // Place a finger on notification.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Move the finger horizontally by 29 px. (29 px is less than the swipe
  // control width).
  PostSequentialGestureEvent(ui::GestureEventDetails(
      ui::ET_GESTURE_SCROLL_UPDATE, kSwipeControlWidth - 1, 0));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Release the finger.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));

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

TEST_F(SlideOutControllerTest, SwipeControlWidth_SwipeMoreThanControlWidth) {
  // Set the width of swipe control.
  slide_out_controller()->SetSwipeControlWidth(kSwipeControlWidth);

  // Place a finger on notification.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Move the finger horizontally by 31 px. (31 px is more than the swipe
  // control width).
  PostSequentialGestureEvent(ui::GestureEventDetails(
      ui::ET_GESTURE_SCROLL_UPDATE, kSwipeControlWidth + 1, 0));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Release the finger.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));

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

TEST_F(SlideOutControllerTest, SetSwipeControlWidth_SwipeOut) {
  // Set the width of swipe control.
  slide_out_controller()->SetSwipeControlWidth(kSwipeControlWidth);

  // Place a finger on notification.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Move the finger horizontally by 101 px. (101 px is more than the half of
  // the target width).
  PostSequentialGestureEvent(ui::GestureEventDetails(
      ui::ET_GESTURE_SCROLL_UPDATE, kTargetWidth / 2 + 1, 0));

  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  delegate()->reset();

  // Release the finger.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));

  // ... and it is automatically slided out.
  EXPECT_EQ(0, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_FALSE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  EXPECT_EQ(kTargetWidth,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Ensure a deferred SlideOut handler is called once.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(1, delegate()->slide_out_count_);
}

TEST_F(SlideOutControllerTest, SwipeControlWidth_SnapAndSwipeOut) {
  // Set the width of swipe control.
  slide_out_controller()->SetSwipeControlWidth(kSwipeControlWidth);

  // Snap to the swipe control.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
  PostSequentialGestureEvent(ui::GestureEventDetails(
      ui::ET_GESTURE_SCROLL_UPDATE, kSwipeControlWidth, 0));
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  EXPECT_EQ(kSwipeControlWidth,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Swipe horizontally by 70 px.
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 70, 0));
  PostSequentialGestureEvent(
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));

  // ... and it is automatically slided out.
  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_TRUE(delegate()->IsOnSlideChangedCalled());
  EXPECT_FALSE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
  EXPECT_EQ(kTargetWidth,
            delegate()->GetSlideOutLayer()->transform().To2dTranslation().x());

  delegate()->reset();

  // Ensure a deferred OnSlideOut handler is called.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate()->IsOnSlideChangedCalled());
  EXPECT_EQ(1, delegate()->slide_out_count_);
}

TEST_F(SlideOutControllerTest, SwipeControlWidth_SnapAndSnapToControl) {
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

TEST_F(SlideOutControllerTest, SwipeControlWidth_SnapAndBackToOrigin) {
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

TEST_F(SlideOutControllerTest, SwipeControlWidth_NotSnapAndBackToOrigin) {
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

}  // namespace views
