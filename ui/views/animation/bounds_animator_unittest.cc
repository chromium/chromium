// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/bounds_animator.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/icu_test_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/test_animation_delegate.h"
#include "ui/views/view.h"

using gfx::Animation;
using gfx::SlideAnimation;
using gfx::TestAnimationDelegate;

namespace views {
namespace {

class OwnedDelegate : public gfx::AnimationDelegate {
 public:
  OwnedDelegate() = default;

  OwnedDelegate(const OwnedDelegate&) = delete;
  OwnedDelegate& operator=(const OwnedDelegate&) = delete;

  ~OwnedDelegate() override { deleted_ = true; }

  static bool GetAndClearDeleted() {
    bool value = deleted_;
    deleted_ = false;
    return value;
  }

  static bool GetAndClearCanceled() {
    bool value = canceled_;
    canceled_ = false;
    return value;
  }

  // Overridden from gfx::AnimationDelegate:
  void AnimationCanceled(const Animation* animation) override {
    canceled_ = true;
  }

 private:
  static bool deleted_;
  static bool canceled_;
};

// static
bool OwnedDelegate::deleted_ = false;
bool OwnedDelegate::canceled_ = false;

class TestView : public View {
  METADATA_HEADER(TestView, View)

 public:
  TestView() = default;

  TestView(const TestView&) = delete;
  TestView& operator=(const TestView&) = delete;

  void OnDidSchedulePaint(const gfx::Rect& r) override {
    ++repaint_count_;

    if (dirty_rect_.IsEmpty())
      dirty_rect_ = r;
    else
      dirty_rect_.Union(r);
  }

  const gfx::Rect& dirty_rect() const { return dirty_rect_; }

  void set_repaint_count(int val) { repaint_count_ = val; }
  int repaint_count() const { return repaint_count_; }

 private:
  gfx::Rect dirty_rect_;
  int repaint_count_ = 0;
};

BEGIN_METADATA(TestView)
END_METADATA

class RTLAnimationTestDelegate : public gfx::AnimationDelegate {
 public:
  RTLAnimationTestDelegate(const gfx::Rect& start,
                           const gfx::Rect& target,
                           View* view,
                           base::RepeatingClosure quit_closure)
      : start_(start),
        target_(target),
        view_(view),
        quit_closure_(std::move(quit_closure)) {}
  ~RTLAnimationTestDelegate() override = default;

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const Animation* animation) override {
    gfx::Transform transform = view_->GetTransform();
    ASSERT_TRUE(!transform.IsIdentity());

    // In this test, assume that |parent| is root view.
    View* parent = view_->parent();

    const gfx::Rect start_rect_in_screen = parent->GetMirroredRect(start_);
    const gfx::Rect target_rect_in_screen = parent->GetMirroredRect(target_);

    gfx::Rect current_bounds_in_screen =
        transform.MapRect(parent->GetMirroredRect(view_->bounds()));

    // Verify that |view_|'s current bounds in screen are valid.
    EXPECT_GE(current_bounds_in_screen.x(),
              std::min(start_rect_in_screen.x(), target_rect_in_screen.x()));
    EXPECT_LE(
        current_bounds_in_screen.right(),
        std::max(start_rect_in_screen.right(), target_rect_in_screen.right()));

    quit_closure_.Run();
  }

  // Animation initial bounds.
  gfx::Rect start_;

  // Animation target bounds.
  gfx::Rect target_;

  // view to be animated.
  raw_ptr<View> view_;

  base::RepeatingClosure quit_closure_;
};

}  // namespace

class BoundsAnimatorTest : public testing::Test {
 public:
  BoundsAnimatorTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {
    parent_.AddChildView(std::make_unique<TestView>());
    RecreateAnimator(/*use_transforms=*/false);
  }

  BoundsAnimatorTest(const BoundsAnimatorTest&) = delete;
  BoundsAnimatorTest& operator=(const BoundsAnimatorTest&) = delete;

  TestView* parent() { return &parent_; }
  TestView* child() {
    return static_cast<TestView*>(parent_.children()[0].get());
  }
  BoundsAnimator* animator() { return animator_.get(); }

 protected:
  void RecreateAnimator(bool use_transforms) {
    animator_ = std::make_unique<BoundsAnimator>(&parent_, use_transforms);
    animator_->SetAnimationDuration(base::Milliseconds(10));
  }

  // Animates |child_| to |target_bounds|. Returns the repaint time.
  // |use_long_duration| indicates whether long or short bounds animation is
  // created.
  int GetRepaintTimeFromBoundsAnimation(const gfx::Rect& target_bounds,
                                        bool use_long_duration) {
    base::RunLoop loop;
    child()->set_repaint_count(0);

    const base::TimeDelta animation_duration =
        base::Milliseconds(use_long_duration ? 2000 : 10);
    animator()->SetAnimationDuration(animation_duration);

    animator()->AnimateViewTo(child(), target_bounds);
    animator()->SetAnimationDelegate(
        child(),
        std::make_unique<TestAnimationDelegate>(loop.QuitWhenIdleClosure()));

    // The animator should be animating now.
    EXPECT_TRUE(animator()->IsAnimating());
    EXPECT_TRUE(animator()->IsAnimating(child()));

    // Run the message loop; the delegate exits the loop when the animation is
    // done.
    if (use_long_duration)
      task_environment_.FastForwardBy(animation_duration);
    loop.Run();

    // Make sure the bounds match of the view that was animated match and the
    // layer is destroyed.
    EXPECT_EQ(target_bounds, child()->bounds());
    EXPECT_FALSE(child()->layer());

    // |child| shouldn't be animating anymore.
    EXPECT_FALSE(animator()->IsAnimating(child()));

    return child()->repaint_count();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  TestView parent_;
  std::unique_ptr<BoundsAnimator> animator_;
};

// Checks animate view to.
TEST_F(BoundsAnimatorTest, AnimateViewTo) {
  base::RunLoop loop;
  gfx::Rect initial_bounds(0, 0, 10, 10);
  child()->SetBoundsRect(initial_bounds);
  gfx::Rect target_bounds(10, 10, 20, 20);
  animator()->AnimateViewTo(child(), target_bounds);
  animator()->SetAnimationDelegate(
      child(),
      std::make_unique<TestAnimationDelegate>(loop.QuitWhenIdleClosure()));

  // The animator should be animating now.
  EXPECT_TRUE(animator()->IsAnimating());
  EXPECT_TRUE(animator()->IsAnimating(child()));

  // Run the message loop; the delegate exits the loop when the animation is
  // done.
  loop.Run();

  // Make sure the bounds match of the view that was animated match.
  EXPECT_EQ(target_bounds, child()->bounds());

  // |child| shouldn't be animating anymore.
  EXPECT_FALSE(animator()->IsAnimating(child()));

  // The parent should have been told to repaint as the animation progressed.
  // The resulting rect is the union of the original and target bounds.
  EXPECT_EQ(gfx::UnionRects(target_bounds, initial_bounds),
            parent()->dirty_rect());
}

// Make sure that removing/deleting a child view while animating stops the
// view's animation and will not result in a crash.
TEST_F(BoundsAnimatorTest, DeleteWhileAnimating) {
  animator()->AnimateViewTo(child(), gfx::Rect(0, 0, 10, 10));
  animator()->SetAnimationDelegate(child(), std::make_unique<OwnedDelegate>());

  EXPECT_TRUE(animator()->IsAnimating(child()));

  // Make sure that animation is removed upon deletion.
  std::unique_ptr<View> child_owning = parent()->RemoveChildViewT(child());
  EXPECT_FALSE(animator()->GetAnimationForView(child_owning.get()));
  EXPECT_FALSE(animator()->IsAnimating(child_owning.get()));
}

// Make sure an AnimationDelegate is deleted when canceled.
TEST_F(BoundsAnimatorTest, DeleteDelegateOnCancel) {
  animator()->AnimateViewTo(child(), gfx::Rect(0, 0, 10, 10));
  animator()->SetAnimationDelegate(child(), std::make_unique<OwnedDelegate>());

  animator()->Cancel();

  // The animator should no longer be animating.
  EXPECT_FALSE(animator()->IsAnimating());
  EXPECT_FALSE(animator()->IsAnimating(child()));

  // The cancel should both cancel the delegate and delete it.
  EXPECT_TRUE(OwnedDelegate::GetAndClearCanceled());
  EXPECT_TRUE(OwnedDelegate::GetAndClearDeleted());
}

// Make sure that the AnimationDelegate of the running animation is deleted when
// a new animation is scheduled.
TEST_F(BoundsAnimatorTest, DeleteDelegateOnNewAnimate) {
  const gfx::Rect target_bounds_first(0, 0, 10, 10);
  animator()->AnimateViewTo(child(), target_bounds_first);
  animator()->SetAnimationDelegate(child(), std::make_unique<OwnedDelegate>());

  // Start an animation on the same view with different target bounds.
  const gfx::Rect target_bounds_second(0, 5, 10, 10);
  animator()->AnimateViewTo(child(), target_bounds_second);

  // Starting a new animation should both cancel the delegate and delete it.
  EXPECT_TRUE(OwnedDelegate::GetAndClearDeleted());
  EXPECT_TRUE(OwnedDelegate::GetAndClearCanceled());
}

// Make sure that the duplicate animation request does not interrupt the running
// animation.
TEST_F(BoundsAnimatorTest, HandleDuplicateAnimation) {
  const gfx::Rect target_bounds(0, 0, 10, 10);

  animator()->AnimateViewTo(child(), target_bounds);
  animator()->SetAnimationDelegate(child(), std::make_unique<OwnedDelegate>());

  // Request the animation with the same view/target bounds.
  animator()->AnimateViewTo(child(), target_bounds);

  // Verify that the existing animation is not interrupted.
  EXPECT_FALSE(OwnedDelegate::GetAndClearDeleted());
  EXPECT_FALSE(OwnedDelegate::GetAndClearCanceled());
}

// Make sure that a duplicate animation request that specifies a different
// delegate swaps out that delegate.
TEST_F(BoundsAnimatorTest, DuplicateAnimationsCanReplaceDelegate) {
  const gfx::Rect target_bounds(0, 0, 10, 10);

  animator()->AnimateViewTo(child(), target_bounds);
  animator()->SetAnimationDelegate(child(), std::make_unique<OwnedDelegate>());

  // Request the animation with the same view/target bounds but a different
  // delegate.
  animator()->AnimateViewTo(child(), target_bounds,
                            std::make_unique<OwnedDelegate>());

  // Verify that the delegate was replaced.
  EXPECT_TRUE(OwnedDelegate::GetAndClearDeleted());
  // The animation still should not have been canceled.
  EXPECT_FALSE(OwnedDelegate::GetAndClearCanceled());
}

// Makes sure StopAnimating works.
TEST_F(BoundsAnimatorTest, StopAnimating) {
  std::unique_ptr<OwnedDelegate> delegate(std::make_unique<OwnedDelegate>());

  animator()->AnimateViewTo(child(), gfx::Rect(0, 0, 10, 10));
  animator()->SetAnimationDelegate(child(), std::make_unique<OwnedDelegate>());

  animator()->StopAnimatingView(child());

  // Shouldn't be animating now.
  EXPECT_FALSE(animator()->IsAnimating());
  EXPECT_FALSE(animator()->IsAnimating(child()));

  // Stopping should both cancel the delegate and delete it.
  EXPECT_TRUE(OwnedDelegate::GetAndClearDeleted());
  EXPECT_TRUE(OwnedDelegate::GetAndClearCanceled());
}

// Make sure Complete completes in-progress animations.
TEST_F(BoundsAnimatorTest, CompleteAnimation) {
  std::unique_ptr<OwnedDelegate> delegate(std::make_unique<OwnedDelegate>());
  const gfx::Rect target_bounds = gfx::Rect(0, 0, 10, 10);

  animator()->AnimateViewTo(child(), target_bounds);
  animator()->SetAnimationDelegate(child(), std::make_unique<OwnedDelegate>());

  animator()->Complete();

  // Shouldn't be animating now.
  EXPECT_FALSE(animator()->IsAnimating());
  EXPECT_FALSE(animator()->IsAnimating(child()));

  // Child should have been moved to the animation's target.
  EXPECT_EQ(target_bounds, child()->bounds());

  // Completing should delete the delegate.
  EXPECT_TRUE(OwnedDelegate::GetAndClearDeleted());
  EXPECT_FALSE(OwnedDelegate::GetAndClearCanceled());
}

// Verify that transform is used when the animation target bounds have the
// same size with the current bounds' meanwhile having the transform option
// enabled.
TEST_F(BoundsAnimatorTest, UseTransformsAnimateViewTo) {
  RecreateAnimator(/*use_transforms=*/true);

  const gfx::Rect initial_bounds(0, 0, 10, 10);
  child()->SetBoundsRect(initial_bounds);

  // Ensure that the target bounds have the same size with the initial bounds'
  // to apply transform to bounds animation.
  const gfx::Rect target_bounds_without_resize(gfx::Point(10, 10),
                                               initial_bounds.size());

  const int repaint_time_from_short_animation =
      GetRepaintTimeFromBoundsAnimation(target_bounds_without_resize,
                                        /*use_long_duration=*/false);
  const int repaint_time_from_long_animation =
      GetRepaintTimeFromBoundsAnimation(initial_bounds,
                                        /*use_long_duration=*/true);

  // The number of repaints in long animation should be the same as with the
  // short animation.
  EXPECT_EQ(repaint_time_from_short_animation,
            repaint_time_from_long_animation);
}

// Verify that transform is not used when the animation target bounds have the
// different size from the current bounds' even if transform is preferred.
TEST_F(BoundsAnimatorTest, NoTransformForScalingAnimation) {
  RecreateAnimator(/*use_transforms=*/true);

  const gfx::Rect initial_bounds(0, 0, 10, 10);
  child()->SetBoundsRect(initial_bounds);

  // Ensure that the target bounds have the different size with the initial
  // bounds' to repaint bounds in each animation tick.
  const gfx::Rect target_bounds_with_reize(gfx::Point(10, 10),
                                           gfx::Size(20, 20));

  const int repaint_time_from_short_animation =
      GetRepaintTimeFromBoundsAnimation(target_bounds_with_reize,
                                        /*use_long_duration=*/false);
  const int repaint_time_from_long_animation =
      GetRepaintTimeFromBoundsAnimation(initial_bounds,
                                        /*use_long_duration=*/true);

  // When creating bounds animation with repaint, the longer bounds animation
  // should have more repaint counts.
  EXPECT_GT(repaint_time_from_long_animation,
            repaint_time_from_short_animation);
}

// Tests that the transforms option does not crash when a view's bounds start
// off empty.
TEST_F(BoundsAnimatorTest, UseTransformsAnimateViewToEmptySrc) {
  base::RunLoop loop;
  RecreateAnimator(/*use_transforms=*/true);

  gfx::Rect initial_bounds(0, 0, 0, 0);
  child()->SetBoundsRect(initial_bounds);
  gfx::Rect target_bounds(10, 10, 20, 20);

  child()->set_repaint_count(0);
  animator()->AnimateViewTo(child(), target_bounds);
  animator()->SetAnimationDelegate(
      child(),
      std::make_unique<TestAnimationDelegate>(loop.QuitWhenIdleClosure()));

  // The animator should be animating now.
  EXPECT_TRUE(animator()->IsAnimating());
  EXPECT_TRUE(animator()->IsAnimating(child()));

  // Run the message loop; the delegate exits the loop when the animation is
  // done.
  loop.Run();
  EXPECT_EQ(target_bounds, child()->bounds());
}

// Tests that when using the transform option on the bounds animator, cancelling
// the animation part way results in the correct bounds applied.
TEST_F(BoundsAnimatorTest, UseTransformsCancelAnimation) {
  RecreateAnimator(/*use_transforms=*/true);

  // Ensure that |initial_bounds| has the same size with |target_bounds| to
  // create bounds animation via the transform.
  const gfx::Rect initial_bounds(0, 0, 10, 10);
  const gfx::Rect target_bounds(10, 10, 10, 10);

  child()->SetBoundsRect(initial_bounds);

  const base::TimeDelta duration = base::Milliseconds(200);
  animator()->SetAnimationDuration(duration);
  // Use a linear tween so we can estimate the expected bounds.
  animator()->set_tween_type(gfx::Tween::LINEAR);
  animator()->AnimateViewTo(child(), target_bounds);
  animator()->SetAnimationDelegate(child(),
                                   std::make_unique<TestAnimationDelegate>());
  EXPECT_TRUE(animator()->IsAnimating());
  EXPECT_TRUE(animator()->IsAnimating(child()));

  // Stop halfway and cancel. The child should have its bounds updated to
  // exactly halfway between |initial_bounds| and |target_bounds|.
  const gfx::Rect expected_bounds(5, 5, 10, 10);
  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(initial_bounds, child()->bounds());
  animator()->Cancel();
  EXPECT_EQ(expected_bounds, child()->bounds());
}

// Test that when using the transform option on the bounds animator, cancelling
// the animation part way under RTL results in the correct bounds applied.
TEST_F(BoundsAnimatorTest, UseTransformsCancelAnimationRTL) {
  // Enable RTL.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale("he");

  RecreateAnimator(/*use_transforms=*/true);

  // Ensure that |initial_bounds| has the same size with |target_bounds| to
  // create bounds animation via the transform.
  const gfx::Rect initial_bounds(0, 0, 10, 10);
  const gfx::Rect target_bounds(10, 10, 10, 10);

  child()->SetBoundsRect(initial_bounds);

  const base::TimeDelta duration = base::Milliseconds(200);
  animator()->SetAnimationDuration(duration);
  // Use a linear tween so we can estimate the expected bounds.
  animator()->set_tween_type(gfx::Tween::LINEAR);
  animator()->AnimateViewTo(child(), target_bounds);
  EXPECT_TRUE(animator()->IsAnimating());
  EXPECT_TRUE(animator()->IsAnimating(child()));

  // Stop halfway and cancel. The child should have its bounds updated to
  // exactly halfway between |initial_bounds| and |target_bounds|.
  const gfx::Rect expected_bounds(5, 5, 10, 10);
  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(initial_bounds, child()->bounds());
  animator()->Cancel();
  EXPECT_EQ(expected_bounds, child()->bounds());
}

// Verify that the bounds animation which updates the transform of views work
// as expected under RTL (https://crbug.com/1067033).
TEST_F(BoundsAnimatorTest, VerifyBoundsAnimatorUnderRTL) {
  // Enable RTL.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale("he");

  RecreateAnimator(/*use_transforms=*/true);
  parent()->SetBounds(0, 0, 40, 40);

  const gfx::Rect initial_bounds(0, 0, 10, 10);
  child()->SetBoundsRect(initial_bounds);
  const gfx::Rect target_bounds(10, 10, 10, 10);

  const base::TimeDelta animation_duration = base::Milliseconds(10);
  animator()->SetAnimationDuration(animation_duration);
  child()->set_repaint_count(0);
  animator()->AnimateViewTo(child(), target_bounds);
  base::RunLoop run_loop;
  animator()->SetAnimationDelegate(
      child(),
      std::make_unique<RTLAnimationTestDelegate>(
          initial_bounds, target_bounds, child(), run_loop.QuitClosure()));

  // The animator should be animating now.
  EXPECT_TRUE(animator()->IsAnimating());
  EXPECT_TRUE(animator()->IsAnimating(child()));

  run_loop.Run();
  EXPECT_FALSE(animator()->IsAnimating(child()));
}

}  // namespace views
