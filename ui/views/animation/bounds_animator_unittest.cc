// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/bounds_animator.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
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

  DISALLOW_COPY_AND_ASSIGN(OwnedDelegate);
};

// static
bool OwnedDelegate::deleted_ = false;
bool OwnedDelegate::canceled_ = false;

class TestView : public View {
 public:
  TestView() = default;

  void OnDidSchedulePaint(const gfx::Rect& r) override {
    if (dirty_rect_.IsEmpty())
      dirty_rect_ = r;
    else
      dirty_rect_.Union(r);
  }

  const gfx::Rect& dirty_rect() const { return dirty_rect_; }

 private:
  gfx::Rect dirty_rect_;

  DISALLOW_COPY_AND_ASSIGN(TestView);
};

}  // namespace

class BoundsAnimatorTest : public testing::Test {
 public:
  BoundsAnimatorTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        child_(new TestView()),
        animator_(&parent_) {
    parent_.AddChildView(child_);
    animator_.SetAnimationDuration(base::TimeDelta::FromMilliseconds(10));
  }

  TestView* parent() { return &parent_; }
  TestView* child() { return child_; }
  BoundsAnimator* animator() { return &animator_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestView parent_;
  TestView* child_;  // Owned by |parent_|.
  BoundsAnimator animator_;

  DISALLOW_COPY_AND_ASSIGN(BoundsAnimatorTest);
};

// Checks animate view to.
TEST_F(BoundsAnimatorTest, AnimateViewTo) {
  gfx::Rect initial_bounds(0, 0, 10, 10);
  child()->SetBoundsRect(initial_bounds);
  gfx::Rect target_bounds(10, 10, 20, 20);
  animator()->AnimateViewTo(child(), target_bounds);
  animator()->SetAnimationDelegate(
      child(),
      std::unique_ptr<gfx::AnimationDelegate>(new TestAnimationDelegate()));

  // The animator should be animating now.
  EXPECT_TRUE(animator()->IsAnimating());

  // Run the message loop; the delegate exits the loop when the animation is
  // done.
  base::RunLoop().Run();

  // Make sure the bounds match of the view that was animated match.
  EXPECT_EQ(target_bounds, child()->bounds());

  // The parent should have been told to repaint as the animation progressed.
  // The resulting rect is the union of the original and target bounds.
  EXPECT_EQ(gfx::UnionRects(target_bounds, initial_bounds),
            parent()->dirty_rect());
}

// Make sure an AnimationDelegate is deleted when canceled.
TEST_F(BoundsAnimatorTest, DeleteDelegateOnCancel) {
  animator()->AnimateViewTo(child(), gfx::Rect(0, 0, 10, 10));
  animator()->SetAnimationDelegate(
      child(), std::unique_ptr<gfx::AnimationDelegate>(new OwnedDelegate()));

  animator()->Cancel();

  // The animator should no longer be animating.
  EXPECT_FALSE(animator()->IsAnimating());

  // The cancel should both cancel the delegate and delete it.
  EXPECT_TRUE(OwnedDelegate::GetAndClearCanceled());
  EXPECT_TRUE(OwnedDelegate::GetAndClearDeleted());
}

// Make sure an AnimationDelegate is deleted when another animation is
// scheduled.
TEST_F(BoundsAnimatorTest, DeleteDelegateOnNewAnimate) {
  animator()->AnimateViewTo(child(), gfx::Rect(0, 0, 10, 10));
  animator()->SetAnimationDelegate(
      child(), std::unique_ptr<gfx::AnimationDelegate>(new OwnedDelegate()));

  animator()->AnimateViewTo(child(), gfx::Rect(0, 0, 10, 10));

  // Starting a new animation should both cancel the delegate and delete it.
  EXPECT_TRUE(OwnedDelegate::GetAndClearDeleted());
  EXPECT_TRUE(OwnedDelegate::GetAndClearCanceled());
}

// Makes sure StopAnimating works.
TEST_F(BoundsAnimatorTest, StopAnimating) {
  std::unique_ptr<OwnedDelegate> delegate(new OwnedDelegate());

  animator()->AnimateViewTo(child(), gfx::Rect(0, 0, 10, 10));
  animator()->SetAnimationDelegate(
      child(), std::unique_ptr<gfx::AnimationDelegate>(new OwnedDelegate()));

  animator()->StopAnimatingView(child());

  // Shouldn't be animating now.
  EXPECT_FALSE(animator()->IsAnimating());

  // Stopping should both cancel the delegate and delete it.
  EXPECT_TRUE(OwnedDelegate::GetAndClearDeleted());
  EXPECT_TRUE(OwnedDelegate::GetAndClearCanceled());
}

}  // namespace views
