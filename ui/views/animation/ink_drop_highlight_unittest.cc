// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_highlight.h"

#include <cmath>
#include <memory>
#include <utility>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/test/ink_drop_highlight_test_api.h"
#include "ui/views/animation/test/test_ink_drop_highlight_observer.h"

namespace views::test {

class InkDropHighlightTest : public testing::Test {
 public:
  InkDropHighlightTest();

  InkDropHighlightTest(const InkDropHighlightTest&) = delete;
  InkDropHighlightTest& operator=(const InkDropHighlightTest&) = delete;

  ~InkDropHighlightTest() override;

 protected:
  InkDropHighlight* ink_drop_highlight() { return ink_drop_highlight_.get(); }

  InkDropHighlightTestApi test_api() {
    return InkDropHighlightTestApi(ink_drop_highlight_.get());
  }

  // Observer of the test target.
  TestInkDropHighlightObserver* observer() { return &observer_; }

  // Initializes |ink_drop_highlight_| and attaches |test_api_| and |observer_|
  // to the new instance.
  void InitHighlight(std::unique_ptr<InkDropHighlight> new_highlight);

  // Destroys the |ink_drop_highlight_| and the attached |test_api_|.
  void DestroyHighlight();

 private:
  // The test target.
  std::unique_ptr<InkDropHighlight> ink_drop_highlight_;

  // Observer of the test target.
  TestInkDropHighlightObserver observer_;

  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
};

InkDropHighlightTest::InkDropHighlightTest()
    : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {
  InitHighlight(std::make_unique<InkDropHighlight>(
      gfx::Size(10, 10), 3, gfx::PointF(), SK_ColorBLACK));
}

InkDropHighlightTest::~InkDropHighlightTest() {
  // Destory highlight to make sure it is destroyed before the observer.
  DestroyHighlight();
}

void InkDropHighlightTest::InitHighlight(
    std::unique_ptr<InkDropHighlight> new_highlight) {
  ink_drop_highlight_ = std::move(new_highlight);
  test_api().SetDisableAnimationTimers(true);
  ink_drop_highlight()->set_observer(&observer_);
}

void InkDropHighlightTest::DestroyHighlight() {
  ink_drop_highlight_.reset();
}

TEST_F(InkDropHighlightTest, InitialStateAfterConstruction) {
  EXPECT_FALSE(ink_drop_highlight()->IsFadingInOrVisible());
}

TEST_F(InkDropHighlightTest, IsHighlightedStateTransitions) {
  ink_drop_highlight()->FadeIn(base::Seconds(1));
  EXPECT_TRUE(ink_drop_highlight()->IsFadingInOrVisible());

  test_api().CompleteAnimations();
  EXPECT_TRUE(ink_drop_highlight()->IsFadingInOrVisible());

  ink_drop_highlight()->FadeOut(base::Seconds(1));
  EXPECT_FALSE(ink_drop_highlight()->IsFadingInOrVisible());

  test_api().CompleteAnimations();
  EXPECT_FALSE(ink_drop_highlight()->IsFadingInOrVisible());
}

TEST_F(InkDropHighlightTest, VerifyObserversAreNotified) {
  // TODO(bruthig): Re-enable! For some reason these tests fail on some win
  // trunk builds. See crbug.com/731811.
  if (!gfx::Animation::ShouldRenderRichAnimation())
    return;

  ink_drop_highlight()->FadeIn(base::Seconds(1));

  EXPECT_EQ(1, observer()->last_animation_started_ordinal());
  EXPECT_FALSE(observer()->AnimationHasEnded());

  test_api().CompleteAnimations();

  EXPECT_TRUE(observer()->AnimationHasEnded());
  EXPECT_EQ(2, observer()->last_animation_ended_ordinal());
}

TEST_F(InkDropHighlightTest,
       VerifyObserversAreNotifiedWithCorrectAnimationType) {
  ink_drop_highlight()->FadeIn(base::Seconds(1));

  EXPECT_TRUE(observer()->AnimationHasStarted());
  EXPECT_EQ(InkDropHighlight::AnimationType::kFadeIn,
            observer()->last_animation_started_context());

  test_api().CompleteAnimations();
  EXPECT_TRUE(observer()->AnimationHasEnded());
  EXPECT_EQ(InkDropHighlight::AnimationType::kFadeIn,
            observer()->last_animation_started_context());

  ink_drop_highlight()->FadeOut(base::Seconds(1));
  EXPECT_EQ(InkDropHighlight::AnimationType::kFadeOut,
            observer()->last_animation_started_context());

  test_api().CompleteAnimations();
  EXPECT_EQ(InkDropHighlight::AnimationType::kFadeOut,
            observer()->last_animation_started_context());
}

TEST_F(InkDropHighlightTest, VerifyObserversAreNotifiedOfSuccessfulAnimations) {
  ink_drop_highlight()->FadeIn(base::Seconds(1));
  test_api().CompleteAnimations();

  EXPECT_EQ(2, observer()->last_animation_ended_ordinal());
  EXPECT_EQ(InkDropAnimationEndedReason::SUCCESS,
            observer()->last_animation_ended_reason());
}

TEST_F(InkDropHighlightTest, VerifyObserversAreNotifiedOfPreemptedAnimations) {
  // TODO(bruthig): Re-enable! For some reason these tests fail on some win
  // trunk builds. See crbug.com/731811.
  if (!gfx::Animation::ShouldRenderRichAnimation())
    return;

  ink_drop_highlight()->FadeIn(base::Seconds(1));
  ink_drop_highlight()->FadeOut(base::Seconds(1));

  EXPECT_EQ(2, observer()->last_animation_ended_ordinal());
  EXPECT_EQ(InkDropHighlight::AnimationType::kFadeIn,
            observer()->last_animation_ended_context());
  EXPECT_EQ(InkDropAnimationEndedReason::PRE_EMPTED,
            observer()->last_animation_ended_reason());
}

// Confirms there is no crash.
TEST_F(InkDropHighlightTest, NullObserverIsSafe) {
  ink_drop_highlight()->set_observer(nullptr);

  ink_drop_highlight()->FadeIn(base::Seconds(1));
  test_api().CompleteAnimations();

  ink_drop_highlight()->FadeOut(base::Milliseconds(0));
  test_api().CompleteAnimations();
  EXPECT_FALSE(ink_drop_highlight()->IsFadingInOrVisible());
}

// Verify animations are aborted during deletion and the
// InkDropHighlightObservers are notified.
TEST_F(InkDropHighlightTest, AnimationsAbortedDuringDeletion) {
  // TODO(bruthig): Re-enable! For some reason these tests fail on some win
  // trunk builds. See crbug.com/731811.
  if (!gfx::Animation::ShouldRenderRichAnimation())
    return;

  ink_drop_highlight()->FadeIn(base::Seconds(1));
  DestroyHighlight();
  EXPECT_EQ(1, observer()->last_animation_started_ordinal());
  EXPECT_EQ(2, observer()->last_animation_ended_ordinal());
  EXPECT_EQ(InkDropHighlight::AnimationType::kFadeIn,
            observer()->last_animation_ended_context());
  EXPECT_EQ(InkDropAnimationEndedReason::PRE_EMPTED,
            observer()->last_animation_ended_reason());
}

// Confirms a zero sized highlight doesn't crash.
TEST_F(InkDropHighlightTest, AnimatingAZeroSizeHighlight) {
  InitHighlight(std::make_unique<InkDropHighlight>(
      gfx::Size(0, 0), 3, gfx::PointF(), SK_ColorBLACK));
  ink_drop_highlight()->FadeOut(base::Milliseconds(0));
}

TEST_F(InkDropHighlightTest, TransformIsPixelAligned) {
  constexpr float kEpsilon = 0.001f;
  constexpr gfx::Size kHighlightSize(10, 10);
  InitHighlight(std::make_unique<InkDropHighlight>(
      kHighlightSize, 3, gfx::PointF(3.5f, 3.5f), SK_ColorYELLOW));
  for (auto dsf : {1.25, 1.33, 1.5, 1.6, 1.75, 1.8, 2.25}) {
    SCOPED_TRACE(testing::Message()
                 << std::endl
                 << "Device Scale Factor: " << dsf << std::endl);
    ink_drop_highlight()->layer()->OnDeviceScaleFactorChanged(dsf);

    gfx::Transform transform = test_api().CalculateTransform();
    gfx::PointF transformed_layer_origin = transform.MapPoint(
        gfx::PointF(ink_drop_highlight()->layer()->bounds().origin()));

    // Apply device scale factor to get the final offset.
    gfx::Transform dsf_transform;
    dsf_transform.Scale(dsf, dsf);
    transformed_layer_origin = dsf_transform.MapPoint(transformed_layer_origin);
    EXPECT_NEAR(transformed_layer_origin.x(),
                std::round(transformed_layer_origin.x()), kEpsilon);
    EXPECT_NEAR(transformed_layer_origin.y(),
                std::round(transformed_layer_origin.y()), kEpsilon);
  }
}

}  // namespace views::test
