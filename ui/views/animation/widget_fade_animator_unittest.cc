// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/widget_fade_animator.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

constexpr base::TimeDelta kFadeDuration = base::Milliseconds(1000);
constexpr base::TimeDelta kHalfFadeDuration = kFadeDuration / 2;

class TestWidgetFadeAnimator : public WidgetFadeAnimator {
 public:
  using WidgetFadeAnimator::WidgetFadeAnimator;
  ~TestWidgetFadeAnimator() override = default;

  void AnimationContainerWasSet(gfx::AnimationContainer* container) override {
    WidgetFadeAnimator::AnimationContainerWasSet(container);
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

class WidgetFadeAnimatorTest : public test::WidgetTest {
 public:
  void SetUp() override {
    test::WidgetTest::SetUp();
    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                               Widget::InitParams::Type::TYPE_WINDOW);
    delegate_ = std::make_unique<TestWidgetFadeAnimator>(widget_.get());
    delegate_->set_fade_in_duration(kFadeDuration);
    delegate_->set_fade_out_duration(kFadeDuration);
    non_zero_duration_.emplace(
        gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  }

  void TearDown() override {
    non_zero_duration_.reset();
    if (widget_ && !widget_->IsClosed()) {
      widget_->CloseNow();
    }
    test::WidgetTest::TearDown();
  }

 protected:
  std::unique_ptr<Widget> widget_;
  std::unique_ptr<TestWidgetFadeAnimator> delegate_;
  std::optional<gfx::ScopedAnimationDurationScaleMode> non_zero_duration_;
};

class WidgetFadeAnimatorSlideTest
    : public WidgetFadeAnimatorTest,
      public testing::WithParamInterface<WidgetFadeAnimator::SlideDirection> {
 public:
  WidgetFadeAnimator::SlideDirection slide_direction() const {
    return GetParam();
  }

  int slide_distance() const { return slide_distance_; }

  gfx::Vector2d GetExpectedDisplacement() const {
    switch (slide_direction()) {
      case WidgetFadeAnimator::SlideDirection::kUp:
        return gfx::Vector2d(0, slide_distance());
      case WidgetFadeAnimator::SlideDirection::kDown:
        return gfx::Vector2d(0, -slide_distance());
      case WidgetFadeAnimator::SlideDirection::kLeading:
        return gfx::Vector2d(
            base::i18n::IsRTL() ? -slide_distance() : slide_distance(), 0);
      case WidgetFadeAnimator::SlideDirection::kTrailing:
        return gfx::Vector2d(
            base::i18n::IsRTL() ? slide_distance() : -slide_distance(), 0);
      default:
        NOTREACHED();
    }
  }

  int slide_distance_ = 10;
};

TEST_F(WidgetFadeAnimatorTest, FadeIn) {
  EXPECT_FALSE(widget_->IsVisible());
  delegate_->FadeIn();
  // Fade in should set visibility and opacity to some small value.
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_TRUE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
}

TEST_P(WidgetFadeAnimatorSlideTest, FadeInWithSlide) {
  const gfx::Rect target_bounds(100, 100, 50, 50);
  widget_->SetBounds(target_bounds);

  delegate_->FadeIn(slide_distance(), slide_direction());

  delegate_->test_api()->IncrementTime(kHalfFadeDuration);

  // The widget should slide along the slide direction. At the end,
  // it should reach the target bounds.
  const double value = gfx::Tween::CalculateValue(delegate_->tween_type(), 0.5);
  const gfx::Rect expected_bounds = gfx::Tween::RectValueBetween(
      value, target_bounds + GetExpectedDisplacement(), target_bounds);
  EXPECT_EQ(expected_bounds, widget_->GetWindowBoundsInScreen());

  // At the end, it should be at the target bounds.
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_EQ(target_bounds, widget_->GetWindowBoundsInScreen());
}

TEST_P(WidgetFadeAnimatorSlideTest, FadeOutWithSlide) {
  const gfx::Rect start_bounds(100, 100, 50, 50);
  widget_->SetBounds(start_bounds);
  widget_->Show();

  delegate_->FadeOut(slide_distance(), slide_direction());

  delegate_->test_api()->IncrementTime(kHalfFadeDuration);

  // The widget should slide opposite the slide direction. At the end,
  // it should be displaced from its starting bounds.
  const double value = gfx::Tween::CalculateValue(delegate_->tween_type(), 0.5);
  const gfx::Rect expected_bounds = gfx::Tween::RectValueBetween(
      value, start_bounds, start_bounds + GetExpectedDisplacement());
  EXPECT_EQ(expected_bounds, widget_->GetWindowBoundsInScreen());

  delegate_->test_api()->IncrementTime(kHalfFadeDuration);

  // At the end, it should be at the target bounds.
  EXPECT_EQ(start_bounds + GetExpectedDisplacement(),
            widget_->GetWindowBoundsInScreen());
}

TEST_P(WidgetFadeAnimatorSlideTest, CancelSlideAndSkipToTarget) {
  const gfx::Rect target_bounds(100, 100, 50, 50);
  widget_->SetBounds(target_bounds);

  delegate_->FadeIn(slide_distance(), slide_direction());
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_NE(target_bounds, widget_->GetWindowBoundsInScreen());

  // Cancel and skip to target.
  delegate_->CancelSlide(true);
  EXPECT_EQ(target_bounds, widget_->GetWindowBoundsInScreen());
}

TEST_P(WidgetFadeAnimatorSlideTest, CancelSlideWithoutSkip) {
  const gfx::Rect target_bounds(100, 100, 50, 50);
  widget_->SetBounds(target_bounds);

  delegate_->FadeIn(slide_distance(), slide_direction());
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_NE(target_bounds, widget_->GetWindowBoundsInScreen());
  delegate_->CancelSlide(false);
  const gfx::Rect intermediate_bounds = widget_->GetWindowBoundsInScreen();
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  // The widget bounds should not have moved if we did not skip
  // the slide to the final position.
  EXPECT_EQ(intermediate_bounds, widget_->GetWindowBoundsInScreen());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WidgetFadeAnimatorSlideTest,
    testing::Values(WidgetFadeAnimator::SlideDirection::kUp,
                    WidgetFadeAnimator::SlideDirection::kDown,
                    WidgetFadeAnimator::SlideDirection::kLeading,
                    WidgetFadeAnimator::SlideDirection::kTrailing));

TEST_F(WidgetFadeAnimatorTest, FadeInAnimationProgressesToEnd) {
  delegate_->FadeIn();
  // Note that there is currently no way to *read* a widget's opacity, so we can
  // only verify that the widget's visibility changes appropriately at the
  // beginning and end of each animation.
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_TRUE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
}

TEST_F(WidgetFadeAnimatorTest, FadeOut) {
  widget_->Show();
  EXPECT_TRUE(widget_->IsVisible());
  delegate_->FadeOut();
  // Fade in should set visibility and opacity to some small value.
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_TRUE(delegate_->IsFadingOut());
}

TEST_F(WidgetFadeAnimatorTest, FadeOutAnimationProgressesToEnd) {
  widget_->Show();
  delegate_->FadeOut();
  // Note that there is currently no way to *read* a widget's opacity, so we can
  // only verify that the widget's visibility changes appropriately at the
  // beginning and end of each animation.
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_TRUE(delegate_->IsFadingOut());
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_FALSE(widget_->IsVisible());
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
}

TEST_F(WidgetFadeAnimatorTest, CancelFadeOutAtStart) {
  widget_->Show();
  delegate_->FadeOut();
  delegate_->CancelFadeOut();
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
}

TEST_F(WidgetFadeAnimatorTest, CancelFadeOutInMiddle) {
  widget_->Show();
  delegate_->FadeOut();
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  delegate_->CancelFadeOut();
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
}

TEST_F(WidgetFadeAnimatorTest, CancelFadeOutAtEndHasNoEffect) {
  widget_->Show();
  delegate_->FadeOut();
  delegate_->test_api()->IncrementTime(kFadeDuration);
  delegate_->CancelFadeOut();
  EXPECT_FALSE(widget_->IsVisible());
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
}

TEST_F(WidgetFadeAnimatorTest, CancelFadeOutHasNoEffectIfFadingIn) {
  delegate_->FadeIn();
  delegate_->CancelFadeOut();
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_TRUE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  delegate_->CancelFadeOut();
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_TRUE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  delegate_->CancelFadeOut();
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
}

TEST_F(WidgetFadeAnimatorTest, FadeOutClosesWidget) {
  delegate_->set_close_on_hide(true);
  widget_->Show();
  delegate_->FadeOut();
  delegate_->test_api()->IncrementTime(kFadeDuration);
  EXPECT_TRUE(widget_->IsClosed());
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
}

TEST_F(WidgetFadeAnimatorTest, WidgetClosedDuringFade) {
  widget_->Show();
  delegate_->FadeOut();
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  widget_->CloseNow();
  EXPECT_FALSE(delegate_->IsFadingOut());
}

TEST_F(WidgetFadeAnimatorTest, WidgetDestroyedDuringFade) {
  widget_->Show();
  delegate_->FadeOut();
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  widget_->Close();
  widget_.reset();
  EXPECT_FALSE(delegate_->IsFadingOut());
}

TEST_F(WidgetFadeAnimatorTest, AnimatorDestroyedDuringFade) {
  delegate_->FadeIn();
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  delegate_.reset();
}

TEST_F(WidgetFadeAnimatorTest, FadeOutInterruptsFadeIn) {
  delegate_->FadeIn();
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  delegate_->FadeOut();
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_TRUE(delegate_->IsFadingOut());
  EXPECT_TRUE(widget_->IsVisible());
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_TRUE(delegate_->IsFadingOut());
  EXPECT_TRUE(widget_->IsVisible());
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
  EXPECT_FALSE(widget_->IsVisible());
}

TEST_F(WidgetFadeAnimatorTest, FadeInInterruptsFadeOut) {
  widget_->Show();
  delegate_->FadeOut();
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  delegate_->FadeIn();
  EXPECT_TRUE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
  EXPECT_TRUE(widget_->IsVisible());
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_TRUE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
  EXPECT_TRUE(widget_->IsVisible());
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_FALSE(delegate_->IsFadingIn());
  EXPECT_FALSE(delegate_->IsFadingOut());
  EXPECT_TRUE(widget_->IsVisible());
}

TEST_F(WidgetFadeAnimatorTest, FadeInCallback) {
  int called_count = 0;
  WidgetFadeAnimator::FadeType anim_type = WidgetFadeAnimator::FadeType::kNone;

  auto subscription =
      delegate_->AddFadeCompleteCallback(base::BindLambdaForTesting(
          [&](WidgetFadeAnimator*,
              WidgetFadeAnimator::FadeType animation_type) {
            ++called_count;
            anim_type = animation_type;
          }));

  delegate_->FadeIn();
  EXPECT_EQ(0, called_count);
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_EQ(0, called_count);
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_EQ(1, called_count);
  EXPECT_EQ(WidgetFadeAnimator::FadeType::kFadeIn, anim_type);
}

TEST_F(WidgetFadeAnimatorTest, FadeOutCallback) {
  int called_count = 0;
  WidgetFadeAnimator::FadeType anim_type = WidgetFadeAnimator::FadeType::kNone;

  auto subscription =
      delegate_->AddFadeCompleteCallback(base::BindLambdaForTesting(
          [&](WidgetFadeAnimator*,
              WidgetFadeAnimator::FadeType animation_type) {
            ++called_count;
            anim_type = animation_type;
          }));

  widget_->Show();
  delegate_->FadeOut();
  EXPECT_EQ(0, called_count);
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_EQ(0, called_count);
  delegate_->test_api()->IncrementTime(kHalfFadeDuration);
  EXPECT_EQ(1, called_count);
  EXPECT_EQ(WidgetFadeAnimator::FadeType::kFadeOut, anim_type);
}

TEST_F(WidgetFadeAnimatorTest, FadeInDefaultIsShowActive) {
  EXPECT_FALSE(widget_->IsVisible());
  delegate_->FadeIn();

  // Widget should be shown and active.
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_TRUE(widget_->IsActive());
}

TEST_F(WidgetFadeAnimatorTest, FadeInShowTypeShowInactive) {
  EXPECT_FALSE(widget_->IsVisible());
  delegate_->set_show_type(WidgetFadeAnimator::WidgetShowType::kShowInactive);
  delegate_->FadeIn();

  // Widget should be shown and inactive.
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(widget_->IsActive());
}

TEST_F(WidgetFadeAnimatorTest, FadeInShowTypeNone) {
  EXPECT_FALSE(widget_->IsVisible());
  delegate_->set_show_type(WidgetFadeAnimator::WidgetShowType::kNone);
  delegate_->FadeIn();

  // Widget should not be shown.
  EXPECT_FALSE(widget_->IsVisible());
}

}  // namespace views
