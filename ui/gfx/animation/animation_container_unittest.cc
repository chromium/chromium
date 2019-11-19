// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation_container.h"

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_container_observer.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/test_animation_delegate.h"

namespace gfx {

namespace {

class FakeAnimationContainerObserver : public AnimationContainerObserver {
 public:
  FakeAnimationContainerObserver()
      : progressed_count_(0),
        empty_(false) {
  }

  int progressed_count() const { return progressed_count_; }
  bool empty() const { return empty_; }

 private:
  void AnimationContainerProgressed(AnimationContainer* container) override {
    progressed_count_++;
  }

  // Invoked when no more animations are being managed by this container.
  void AnimationContainerEmpty(AnimationContainer* container) override {
    empty_ = true;
  }

  void AnimationContainerShuttingDown(AnimationContainer* container) override {}

  int progressed_count_;
  bool empty_;

  DISALLOW_COPY_AND_ASSIGN(FakeAnimationContainerObserver);
};

class TestAnimation : public LinearAnimation {
 public:
  explicit TestAnimation(AnimationDelegate* delegate)
      : LinearAnimation(base::TimeDelta::FromMilliseconds(20), 20, delegate) {}

  void AnimateToState(double state) override {}

  using LinearAnimation::duration;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAnimation);
};

}  // namespace

class AnimationContainerTest: public testing::Test {
 protected:
  AnimationContainerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Makes sure the animation ups the ref count of the container and releases it
// appropriately.
TEST_F(AnimationContainerTest, Ownership) {
  TestAnimationDelegate delegate;
  scoped_refptr<AnimationContainer> container(new AnimationContainer());
  std::unique_ptr<Animation> animation(new TestAnimation(&delegate));
  animation->SetContainer(container.get());
  // Setting the container should up the ref count.
  EXPECT_FALSE(container->HasOneRef());

  animation.reset();

  // Releasing the animation should decrement the ref count.
  EXPECT_TRUE(container->HasOneRef());
}

// Makes sure multiple animations are managed correctly.
TEST_F(AnimationContainerTest, Multi) {
  TestAnimationDelegate delegate1;
  TestAnimationDelegate delegate2;

  scoped_refptr<AnimationContainer> container(new AnimationContainer());
  TestAnimation animation1(&delegate1);
  TestAnimation animation2(&delegate2);
  animation1.SetContainer(container.get());
  animation2.SetContainer(container.get());

  // Start both animations.
  animation1.Start();
  EXPECT_TRUE(container->is_running());
  animation2.Start();
  EXPECT_TRUE(container->is_running());

  // Run the message loop the delegate quits the message loop when notified.
  base::RunLoop().Run();

  // Both timers should have finished.
  EXPECT_TRUE(delegate1.finished());
  EXPECT_TRUE(delegate2.finished());

  // And the container should no longer be runnings.
  EXPECT_FALSE(container->is_running());
}

// Makes sure observer is notified appropriately.
TEST_F(AnimationContainerTest, Observer) {
  FakeAnimationContainerObserver observer;
  TestAnimationDelegate delegate1;

  scoped_refptr<AnimationContainer> container(new AnimationContainer());
  container->set_observer(&observer);
  TestAnimation animation1(&delegate1);
  animation1.SetContainer(container.get());

  // Start the animation.
  animation1.Start();
  EXPECT_TRUE(container->is_running());

  // Run the message loop. The delegate quits the message loop when notified.
  base::RunLoop().Run();

  EXPECT_EQ(1, observer.progressed_count());

  // The timer should have finished.
  EXPECT_TRUE(delegate1.finished());

  EXPECT_TRUE(observer.empty());

  // And the container should no longer be running.
  EXPECT_FALSE(container->is_running());

  container->set_observer(NULL);
}

// Tests that calling SetAnimationRunner() keeps running animations at their
// current point.
TEST_F(AnimationContainerTest, AnimationsRunAcrossRunnerChange) {
  TestAnimationDelegate delegate;
  auto container = base::MakeRefCounted<AnimationContainer>();
  AnimationContainerTestApi test_api(container.get());
  TestAnimation animation(&delegate);
  animation.SetContainer(container.get());

  animation.Start();
  test_api.IncrementTime(animation.duration() / 2);
  EXPECT_FALSE(delegate.finished());

  container->SetAnimationRunner(nullptr);
  AnimationRunner* runner = container->animation_runner_for_testing();
  ASSERT_TRUE(runner);
  ASSERT_FALSE(runner->step_is_null_for_testing());
  EXPECT_FALSE(delegate.finished());

  test_api.IncrementTime(animation.duration() / 2);
  EXPECT_TRUE(delegate.finished());
}

}  // namespace gfx
