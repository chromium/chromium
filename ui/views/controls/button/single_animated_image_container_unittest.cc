// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/single_animated_image_container.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/test/skia_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

// Test sub-class of animated image for testing.
class TestSingleAnimatedImageContainer : public SingleAnimatedImageContainer {
 public:
  using SingleAnimatedImageContainer::SingleAnimatedImageContainer;

  TestSingleAnimatedImageContainer(const TestSingleAnimatedImageContainer&) =
      delete;
  TestSingleAnimatedImageContainer& operator=(
      const TestSingleAnimatedImageContainer&) = delete;
  ~TestSingleAnimatedImageContainer() override = default;

  gfx::SlideAnimation& slide_animation() { return slide_animation_; }

  int GetAnimatedImagesCount() const { return animated_images_.size(); }

 protected:
  std::unique_ptr<lottie::Animation> LoadAnimatedImage(
      int resource_id) override {
    return std::make_unique<lottie::Animation>(
        cc::CreateSkottie(gfx::Size(80, 80), /*duration_secs=*/1));
  }
};

class SingleAnimatedImageContainerTest : public ViewsTestBase {
 public:
  SingleAnimatedImageContainerTest()
      : ViewsTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~SingleAnimatedImageContainerTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
    widget_->Init(std::move(params));

    button_ = widget_->SetContentsView(std::make_unique<LabelButton>());
    container_ = std::make_unique<TestSingleAnimatedImageContainer>(button_);
  }

  void TearDown() override {
    container_.reset();
    button_ = nullptr;
    if (widget_) {
      widget_->CloseNow();
      widget_.reset();
    }
    ViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<Widget> widget_;
  raw_ptr<LabelButton> button_;
  base::TimeDelta animation_duration_;
  std::unique_ptr<TestSingleAnimatedImageContainer> container_;
};

TEST_F(SingleAnimatedImageContainerTest, InitializationState) {
  EXPECT_FALSE(container_->slide_animation().is_animating());
  EXPECT_EQ(container_->slide_animation().GetCurrentValue(), 0.0f);
  EXPECT_FALSE(container_->IsShowingAnimation());
  EXPECT_EQ(container_->animation_progress(), std::nullopt);
}

TEST_F(SingleAnimatedImageContainerTest, PlayAnimationForward) {
  SingleAnimatedImageContainer::AnimationDefinition def{/*resource_id=*/100,
                                                        /*color=*/SK_ColorRED};
  SingleAnimatedImageContainer::AnimationConfig config{
      SingleAnimatedImageContainer::AnimationDirection::kForward,
      SingleAnimatedImageContainer::AnimationEndBehavior::kReset};

  container_->PlayAnimation(def, config);

  EXPECT_TRUE(container_->slide_animation().is_animating());
  EXPECT_TRUE(container_->IsShowingAnimation());
  EXPECT_TRUE(container_->HasAnimatedImage(100));

  // Fast forward past the duration to trigger completion.
  task_environment()->FastForwardBy(base::Milliseconds(500));

  // EndBehavior::kReset should automatically drop the animation back to 0.0f.
  EXPECT_FALSE(container_->slide_animation().is_animating());
  EXPECT_EQ(container_->slide_animation().GetCurrentValue(), 0.0f);
  EXPECT_FALSE(container_->IsShowingAnimation());
}

TEST_F(SingleAnimatedImageContainerTest, PlayAnimationBackward) {
  SingleAnimatedImageContainer::AnimationDefinition def{/*resource_id=*/102,
                                                        /*color=*/SK_ColorBLUE};
  SingleAnimatedImageContainer::AnimationConfig config{
      SingleAnimatedImageContainer::AnimationDirection::kBackward,
      SingleAnimatedImageContainer::AnimationEndBehavior::kReset};

  container_->PlayAnimation(def, config);

  // Backward direction sets initial progress directly to 1.0f and runs down.
  EXPECT_TRUE(container_->slide_animation().is_animating());
  EXPECT_EQ(container_->slide_animation().GetCurrentValue(), 1.0f);

  // Run to absolute completion.
  task_environment()->FastForwardBy(base::Milliseconds(500));

  EXPECT_FALSE(container_->slide_animation().is_animating());
  EXPECT_EQ(container_->slide_animation().GetCurrentValue(), 0.0f);
}

TEST_F(SingleAnimatedImageContainerTest, ClearAnimatedImages) {
  SingleAnimatedImageContainer::AnimationDefinition def{
      /*resource_id=*/103,
      /*color=*/SK_ColorYELLOW};
  SingleAnimatedImageContainer::AnimationConfig config{
      SingleAnimatedImageContainer::AnimationDirection::kForward,
      SingleAnimatedImageContainer::AnimationEndBehavior::kReset};

  container_->PlayAnimation(def, config);
  EXPECT_EQ(container_->GetAnimatedImagesCount(), 1);

  container_->ClearAnimatedImages();
  EXPECT_EQ(container_->GetAnimatedImagesCount(), 0);
  EXPECT_EQ(container_->slide_animation().GetCurrentValue(), 0.0f);
}

TEST_F(SingleAnimatedImageContainerTest, PauseAnimationAtEnd) {
  SingleAnimatedImageContainer::AnimationDefinition def{
      /*resource_id=*/101,
      /*color=*/SK_ColorGREEN};
  // Use kPause so the container freezes on the last frame instead of
  // auto-resetting.
  SingleAnimatedImageContainer::AnimationConfig config{
      SingleAnimatedImageContainer::AnimationDirection::kForward,
      SingleAnimatedImageContainer::AnimationEndBehavior::kPause};

  container_->PlayAnimation(def, config);
  EXPECT_TRUE(container_->slide_animation().is_animating());

  // Fast forward past execution limits.
  task_environment()->FastForwardBy(base::Milliseconds(500));

  EXPECT_FALSE(container_->slide_animation().is_animating());
  EXPECT_EQ(container_->slide_animation().GetCurrentValue(), 1.0f);
  EXPECT_TRUE(container_->IsShowingAnimation());
}

TEST_F(SingleAnimatedImageContainerTest, ResetAnimation) {
  SingleAnimatedImageContainer::AnimationDefinition def{/*resource_id=*/102,
                                                        /*color=*/SK_ColorBLUE};
  SingleAnimatedImageContainer::AnimationConfig config{
      SingleAnimatedImageContainer::AnimationDirection::kBackward,
      SingleAnimatedImageContainer::AnimationEndBehavior::kReset};

  container_->PlayAnimation(def, config);

  // Backward direction sets initial progress directly to 1.0f and runs down.
  EXPECT_TRUE(container_->slide_animation().is_animating());
  EXPECT_EQ(container_->slide_animation().GetCurrentValue(), 1.0f);

  container_->ResetAnimation();
  EXPECT_EQ(container_->slide_animation().GetCurrentValue(), 0.0f);
  EXPECT_FALSE(container_->IsShowingAnimation());
}

TEST_F(SingleAnimatedImageContainerTest, AnimationBoundaryOffsets) {
  SingleAnimatedImageContainer::AnimationDefinition def{/*resource_id=*/100,
                                                        /*color=*/SK_ColorRED};
  SingleAnimatedImageContainer::AnimationConfig config;
  config.direction = SingleAnimatedImageContainer::AnimationDirection::kForward;
  config.end_behavior =
      SingleAnimatedImageContainer::AnimationEndBehavior::kReset;
  config.boundary = SingleAnimatedImageContainer::AnimationBoundary{
      .start_offset = 0.25f, .end_offset = 0.75f};

  container_->PlayAnimation(def, config);

  // At the start of forward animation, current value of slide animation is
  // 0.0f. animation_progress() should be mapped to start_offset (0.25f).
  EXPECT_EQ(container_->animation_progress(), 0.25f);

  // Set the slide animation value directly to 0.5f (midway).
  // animation_progress() should be midway between 0.25f and 0.75f, which is
  // 0.5f.
  container_->slide_animation().Reset(0.5f);
  EXPECT_EQ(container_->animation_progress(), 0.5f);

  // Set the slide animation value directly to 1.0f (end).
  // animation_progress() should be end_offset (0.75f).
  container_->slide_animation().Reset(1.0f);
  EXPECT_EQ(container_->animation_progress(), 0.75f);
}

TEST_F(SingleAnimatedImageContainerTest, AnimationBoundaryOffsetsBackward) {
  SingleAnimatedImageContainer::AnimationDefinition def{/*resource_id=*/100,
                                                        /*color=*/SK_ColorRED};
  SingleAnimatedImageContainer::AnimationConfig config;
  config.direction =
      SingleAnimatedImageContainer::AnimationDirection::kBackward;
  config.end_behavior =
      SingleAnimatedImageContainer::AnimationEndBehavior::kReset;
  config.boundary = SingleAnimatedImageContainer::AnimationBoundary{
      .start_offset = 0.25f, .end_offset = 0.75f};

  container_->PlayAnimation(def, config);

  // For backward animation, slide animation starts at 1.0f and goes to 0.0f.
  // At the start (1.0f), progress is end_offset (0.75f).
  EXPECT_EQ(container_->slide_animation().GetCurrentValue(), 1.0f);
  EXPECT_EQ(container_->animation_progress(), 0.75f);

  // Midway (0.5f), progress is 0.5f.
  container_->slide_animation().Reset(0.5f);
  EXPECT_EQ(container_->animation_progress(), 0.5f);

  // At the end (0.0f), progress is start_offset (0.25f).
  container_->slide_animation().Reset(0.0f);
  EXPECT_EQ(container_->animation_progress(), 0.25f);
}

}  // namespace views
