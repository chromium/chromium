// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/bubble_slide_animator.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

constexpr base::TimeDelta kSlideDuration = base::Milliseconds(1000);
constexpr base::TimeDelta kHalfSlideDuration = kSlideDuration / 2;

// This will be the size of the three horizontally-oriented anchor views as well
// as the target size for the floating view.
constexpr gfx::Size kTestViewSize(100, 100);
// Make this big enough that even if we anchor to a third view horizontally, no
// mirroring should happen.
constexpr gfx::Rect kAnchorWidgetRect(50, 50, 400, 250);

class TestBubbleView : public BubbleDialogDelegateView {
 public:
  explicit TestBubbleView(View* anchor_view)
      : BubbleDialogDelegateView(anchor_view,
                                 BubbleBorder::TOP_LEFT,
                                 BubbleBorder::DIALOG_SHADOW,
                                 true) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    SetLayoutManager(std::make_unique<FillLayout>());
    AddChildView(std::make_unique<View>())->SetPreferredSize(kTestViewSize);
  }
};

class TestBubbleSlideAnimator : public BubbleSlideAnimator {
 public:
  using BubbleSlideAnimator::BubbleSlideAnimator;
  ~TestBubbleSlideAnimator() override = default;

  void AnimationContainerWasSet(gfx::AnimationContainer* container) override {
    BubbleSlideAnimator::AnimationContainerWasSet(container);
    container_test_api_.reset();
    if (container) {
      container_test_api_ =
          std::make_unique<gfx::AnimationContainerTestApi>(container);
    }
  }

  gfx::AnimationContainerTestApi* test_api() {
    return container_test_api_.get();
  }

 private:
  std::unique_ptr<gfx::AnimationContainerTestApi> container_test_api_;
};

}  // namespace

class BubbleSlideAnimatorTest : public test::WidgetTest {
 public:
  void SetUp() override {
    test::WidgetTest::SetUp();
    anchor_widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                      Widget::InitParams::Type::TYPE_WINDOW);
    auto* const contents_view = anchor_widget_->GetRootView()->AddChildView(
        std::make_unique<FlexLayoutView>());
    contents_view->SetOrientation(LayoutOrientation::kHorizontal);
    contents_view->SetMainAxisAlignment(LayoutAlignment::kStart);
    contents_view->SetCrossAxisAlignment(LayoutAlignment::kStart);
    view1_ = contents_view->AddChildView(std::make_unique<View>());
    view2_ = contents_view->AddChildView(std::make_unique<View>());
    view3_ = contents_view->AddChildView(std::make_unique<View>());
    view1_->SetPreferredSize(kTestViewSize);
    view2_->SetPreferredSize(kTestViewSize);
    view3_->SetPreferredSize(kTestViewSize);
    anchor_widget_->Show();
    anchor_widget_->SetBounds(kAnchorWidgetRect);
    bubble_ = new TestBubbleView(view1_);
    widget_ = BubbleDialogDelegateView::CreateBubble(bubble_);
    delegate_ = std::make_unique<TestBubbleSlideAnimator>(bubble_);
    delegate_->SetSlideDuration(kSlideDuration);
  }

  void TearDown() override {
    CloseWidget();
    if (anchor_widget_ && !anchor_widget_->IsClosed())
      anchor_widget_->CloseNow();
    test::WidgetTest::TearDown();
  }

  void CloseWidget() {
    bubble_ = nullptr;
    if (widget_ && !widget_->IsClosed()) {
      widget_.ExtractAsDangling()->CloseNow();
    }
  }

 protected:
  std::unique_ptr<Widget> anchor_widget_;
  raw_ptr<BubbleDialogDelegateView> bubble_ = nullptr;
  raw_ptr<Widget> widget_ = nullptr;
  raw_ptr<View> view1_;
  raw_ptr<View> view2_;
  raw_ptr<View> view3_;
  std::unique_ptr<TestBubbleSlideAnimator> delegate_;
};

TEST_F(BubbleSlideAnimatorTest, InitiateSlide) {
  const auto bounds = widget_->GetWindowBoundsInScreen();
  delegate_->AnimateToAnchorView(view2_);
  // Shouldn't animate from here yet.
  EXPECT_EQ(bounds, widget_->GetWindowBoundsInScreen());
  EXPECT_TRUE(delegate_->is_animating());
}

TEST_F(BubbleSlideAnimatorTest, SlideProgresses) {
  const auto starting_bounds = widget_->GetWindowBoundsInScreen();
  delegate_->AnimateToAnchorView(view2_);
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);
  const auto intermediate_bounds = widget_->GetWindowBoundsInScreen();
  EXPECT_TRUE(delegate_->is_animating());
  EXPECT_EQ(intermediate_bounds.y(), starting_bounds.y());
  EXPECT_GT(intermediate_bounds.x(), starting_bounds.x());
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);
  const auto final_bounds = widget_->GetWindowBoundsInScreen();
  EXPECT_FALSE(delegate_->is_animating());
  EXPECT_EQ(final_bounds.y(), starting_bounds.y());
  EXPECT_GT(final_bounds.x(), intermediate_bounds.x());
  EXPECT_EQ(final_bounds.x(), starting_bounds.x() + view2_->x() - view1_->x());
}

TEST_F(BubbleSlideAnimatorTest, SnapToAnchorView) {
  const auto starting_bounds = widget_->GetWindowBoundsInScreen();
  delegate_->SnapToAnchorView(view2_);
  const auto final_bounds = widget_->GetWindowBoundsInScreen();
  EXPECT_FALSE(delegate_->is_animating());
  EXPECT_EQ(final_bounds.y(), starting_bounds.y());
  EXPECT_EQ(final_bounds.x(), starting_bounds.x() + view2_->x() - view1_->x());
}

TEST_F(BubbleSlideAnimatorTest, SlideCallbacksCalled) {
  int progress_count = 0;
  int complete_count = 0;
  double last_progress = 0.0;
  auto progress_sub = delegate_->AddSlideProgressedCallback(
      base::BindLambdaForTesting([&](BubbleSlideAnimator*, double progress) {
        last_progress = progress;
        ++progress_count;
      }));
  auto completed_sub =
      delegate_->AddSlideCompleteCallback(base::BindLambdaForTesting(
          [&](BubbleSlideAnimator*) { ++complete_count; }));
  delegate_->AnimateToAnchorView(view2_);
  EXPECT_EQ(0, progress_count);
  EXPECT_EQ(0, complete_count);
  EXPECT_EQ(0.0, last_progress);
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);
  EXPECT_EQ(1, progress_count);
  EXPECT_EQ(0, complete_count);
  EXPECT_GT(last_progress, 0.0);
  EXPECT_LT(last_progress, 1.0);
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);
  EXPECT_EQ(2, progress_count);
  EXPECT_EQ(1, complete_count);
  EXPECT_EQ(1.0, last_progress);
}

TEST_F(BubbleSlideAnimatorTest, SnapCallbacksCalled) {
  int progress_count = 0;
  int complete_count = 0;
  double last_progress = 0.0;
  auto progress_sub = delegate_->AddSlideProgressedCallback(
      base::BindLambdaForTesting([&](BubbleSlideAnimator*, double progress) {
        last_progress = progress;
        ++progress_count;
      }));
  auto completed_sub =
      delegate_->AddSlideCompleteCallback(base::BindLambdaForTesting(
          [&](BubbleSlideAnimator*) { ++complete_count; }));
  delegate_->SnapToAnchorView(view2_);
  EXPECT_EQ(1, progress_count);
  EXPECT_EQ(1, complete_count);
  EXPECT_EQ(1.0, last_progress);
}

TEST_F(BubbleSlideAnimatorTest, InterruptingWithSlideCallsCorrectCallbacks) {
  int progress_count = 0;
  int complete_count = 0;
  double last_progress = 0.0;
  auto progress_sub = delegate_->AddSlideProgressedCallback(
      base::BindLambdaForTesting([&](BubbleSlideAnimator*, double progress) {
        last_progress = progress;
        ++progress_count;
      }));
  auto completed_sub =
      delegate_->AddSlideCompleteCallback(base::BindLambdaForTesting(
          [&](BubbleSlideAnimator*) { ++complete_count; }));
  delegate_->AnimateToAnchorView(view2_);
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);
  EXPECT_EQ(1, progress_count);
  EXPECT_EQ(0, complete_count);
  delegate_->AnimateToAnchorView(view3_);
  EXPECT_EQ(1, progress_count);
  EXPECT_EQ(0, complete_count);
  delegate_->test_api()->IncrementTime(kSlideDuration);
  EXPECT_EQ(2, progress_count);
  EXPECT_EQ(1, complete_count);
}

TEST_F(BubbleSlideAnimatorTest, InterruptingWithSnapCallsCorrectCallbacks) {
  int progress_count = 0;
  int complete_count = 0;
  double last_progress = 0.0;
  auto progress_sub = delegate_->AddSlideProgressedCallback(
      base::BindLambdaForTesting([&](BubbleSlideAnimator*, double progress) {
        last_progress = progress;
        ++progress_count;
      }));
  auto completed_sub =
      delegate_->AddSlideCompleteCallback(base::BindLambdaForTesting(
          [&](BubbleSlideAnimator*) { ++complete_count; }));
  delegate_->AnimateToAnchorView(view2_);
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);
  EXPECT_EQ(1, progress_count);
  EXPECT_EQ(0, complete_count);
  delegate_->SnapToAnchorView(view3_);
  EXPECT_EQ(2, progress_count);
  EXPECT_EQ(1, complete_count);
  EXPECT_EQ(1.0, last_progress);
}

TEST_F(BubbleSlideAnimatorTest, CancelAnimation) {
  int progress_count = 0;
  int complete_count = 0;
  double last_progress = 0.0;
  auto progress_sub = delegate_->AddSlideProgressedCallback(
      base::BindLambdaForTesting([&](BubbleSlideAnimator*, double progress) {
        last_progress = progress;
        ++progress_count;
      }));
  auto completed_sub =
      delegate_->AddSlideCompleteCallback(base::BindLambdaForTesting(
          [&](BubbleSlideAnimator*) { ++complete_count; }));

  const auto initial_bounds = widget_->GetWindowBoundsInScreen();
  delegate_->AnimateToAnchorView(view2_);
  delegate_->test_api()->IncrementTime(kSlideDuration);
  const auto second_bounds = widget_->GetWindowBoundsInScreen();
  delegate_->AnimateToAnchorView(view1_);
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);
  const auto final_bounds = widget_->GetWindowBoundsInScreen();
  delegate_->StopAnimation();
  EXPECT_FALSE(delegate_->is_animating());
  EXPECT_EQ(2, progress_count);
  EXPECT_EQ(1, complete_count);
  EXPECT_GT(last_progress, 0.0);
  EXPECT_LT(last_progress, 1.0);
  EXPECT_GT(final_bounds.x(), initial_bounds.x());
  EXPECT_LT(final_bounds.x(), second_bounds.x());
}

TEST_F(BubbleSlideAnimatorTest, MultipleSlidesInSequence) {
  // First slide.
  delegate_->AnimateToAnchorView(view2_);
  delegate_->test_api()->IncrementTime(kSlideDuration);
  const auto first_bounds = widget_->GetWindowBoundsInScreen();
  EXPECT_FALSE(delegate_->is_animating());

  // Second slide.
  delegate_->AnimateToAnchorView(view3_);
  EXPECT_TRUE(delegate_->is_animating());
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);

  // Ensure we are sliding.
  const auto intermediate_bounds = widget_->GetWindowBoundsInScreen();
  EXPECT_TRUE(delegate_->is_animating());
  EXPECT_EQ(intermediate_bounds.y(), first_bounds.y());
  EXPECT_GT(intermediate_bounds.x(), first_bounds.x());
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);

  // Ensure we're done.
  const auto final_bounds = widget_->GetWindowBoundsInScreen();
  EXPECT_FALSE(delegate_->is_animating());
  EXPECT_EQ(final_bounds.y(), first_bounds.y());
  EXPECT_EQ(final_bounds.x(), first_bounds.x() + view3_->x() - view2_->x());
}

TEST_F(BubbleSlideAnimatorTest, SlideBackToStartingPosition) {
  const auto first_bounds = widget_->GetWindowBoundsInScreen();
  delegate_->AnimateToAnchorView(view3_);
  delegate_->test_api()->IncrementTime(kSlideDuration);
  delegate_->AnimateToAnchorView(view1_);
  delegate_->test_api()->IncrementTime(kSlideDuration);
  const auto final_bounds = widget_->GetWindowBoundsInScreen();
  EXPECT_FALSE(delegate_->is_animating());
  EXPECT_EQ(final_bounds, first_bounds);
}

TEST_F(BubbleSlideAnimatorTest, InterruptingSlide) {
  const auto starting_bounds = widget_->GetWindowBoundsInScreen();

  // Start the first slide.
  delegate_->AnimateToAnchorView(view2_);
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);
  const auto intermediate_bounds1 = widget_->GetWindowBoundsInScreen();
  EXPECT_TRUE(delegate_->is_animating());

  // Interrupt mid-slide with another slide.
  delegate_->AnimateToAnchorView(view3_);
  EXPECT_TRUE(delegate_->is_animating());
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);

  // Ensure we are sliding.
  const auto intermediate_bounds2 = widget_->GetWindowBoundsInScreen();
  EXPECT_TRUE(delegate_->is_animating());
  EXPECT_EQ(intermediate_bounds2.y(), intermediate_bounds1.y());
  EXPECT_GT(intermediate_bounds2.x(), intermediate_bounds1.x());
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);

  // Ensure we are done.
  const auto final_bounds = widget_->GetWindowBoundsInScreen();
  EXPECT_FALSE(delegate_->is_animating());
  EXPECT_EQ(final_bounds.y(), starting_bounds.y());
  EXPECT_EQ(final_bounds.x(), starting_bounds.x() + view3_->x() - view1_->x());
}

TEST_F(BubbleSlideAnimatorTest, WidgetClosedDuringSlide) {
  delegate_->AnimateToAnchorView(view2_);
  CloseWidget();
  EXPECT_FALSE(delegate_->is_animating());
}

TEST_F(BubbleSlideAnimatorTest, AnimatorDestroyedDuringSlide) {
  delegate_->AnimateToAnchorView(view2_);
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);
  delegate_.reset();
}

TEST_F(BubbleSlideAnimatorTest, AnimationSetsAnchorView) {
  delegate_->AnimateToAnchorView(view2_);
  delegate_->test_api()->IncrementTime(kSlideDuration);
  EXPECT_EQ(view2_, bubble_->GetAnchorView());
  delegate_->AnimateToAnchorView(view3_);
  delegate_->test_api()->IncrementTime(kSlideDuration);
  EXPECT_EQ(view3_, bubble_->GetAnchorView());
}

TEST_F(BubbleSlideAnimatorTest, SnapSetsAnchorView) {
  delegate_->SnapToAnchorView(view2_);
  EXPECT_EQ(view2_, bubble_->GetAnchorView());
  delegate_->SnapToAnchorView(view3_);
  EXPECT_EQ(view3_, bubble_->GetAnchorView());
}

TEST_F(BubbleSlideAnimatorTest, CancelDoesntSetAnchorView) {
  delegate_->AnimateToAnchorView(view2_);
  delegate_->test_api()->IncrementTime(kHalfSlideDuration);
  delegate_->StopAnimation();
  EXPECT_EQ(view1_, bubble_->GetAnchorView());
}

}  // namespace views
