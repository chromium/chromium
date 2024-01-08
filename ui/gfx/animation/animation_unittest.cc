// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/test_animation_delegate.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace gfx {

class AnimationTest : public testing::Test {
 protected:
  AnimationTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

namespace {

///////////////////////////////////////////////////////////////////////////////
// RunAnimation

class RunAnimation : public LinearAnimation {
 public:
  RunAnimation(int frame_rate, AnimationDelegate* delegate)
      : LinearAnimation(delegate, frame_rate) {}

  void AnimateToState(double state) override {
    EXPECT_LE(0.0, state);
    EXPECT_GE(1.0, state);
  }
};

///////////////////////////////////////////////////////////////////////////////
// CancelAnimation

class CancelAnimation : public LinearAnimation {
 public:
  CancelAnimation(base::TimeDelta duration,
                  int frame_rate,
                  AnimationDelegate* delegate)
      : LinearAnimation(duration, frame_rate, delegate) {}

  void AnimateToState(double state) override {
    if (state >= 0.5)
      Stop();
  }
};

///////////////////////////////////////////////////////////////////////////////
// EndAnimation

class EndAnimation : public LinearAnimation {
 public:
  EndAnimation(base::TimeDelta duration,
               int frame_rate,
               AnimationDelegate* delegate)
      : LinearAnimation(duration, frame_rate, delegate) {}

  void AnimateToState(double state) override {
    if (state >= 0.5)
      End();
  }
};

///////////////////////////////////////////////////////////////////////////////
// DeletingAnimationDelegate

// AnimationDelegate implementation that deletes the animation in ended.
class DeletingAnimationDelegate : public TestAnimationDelegate {
 public:
  void AnimationEnded(const Animation* animation) override {
    delete animation;
    QuitRunLoop();
  }
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// LinearCase

TEST_F(AnimationTest, RunCase) {
  base::RunLoop loop;
  TestAnimationDelegate ad;
  ad.set_quit_closure(loop.QuitWhenIdleClosure());
  RunAnimation a1(150, &ad);
  a1.SetDuration(base::Seconds(2));
  a1.Start();
  loop.Run();

  EXPECT_TRUE(ad.finished());
  EXPECT_FALSE(ad.canceled());
}

TEST_F(AnimationTest, CancelCase) {
  base::RunLoop loop;
  TestAnimationDelegate ad;
  ad.set_quit_closure(loop.QuitWhenIdleClosure());
  CancelAnimation a2(base::Seconds(2), 150, &ad);
  a2.Start();
  loop.Run();

  EXPECT_TRUE(ad.finished());
  EXPECT_TRUE(ad.canceled());
}

// Lets an animation run, invoking End part way through and make sure we get the
// right delegate methods invoked.
TEST_F(AnimationTest, EndCase) {
  base::RunLoop loop;
  TestAnimationDelegate ad;
  ad.set_quit_closure(loop.QuitWhenIdleClosure());
  EndAnimation a2(base::Seconds(2), 150, &ad);
  a2.Start();
  loop.Run();

  EXPECT_TRUE(ad.finished());
  EXPECT_FALSE(ad.canceled());
}

// Runs an animation with a delegate that deletes the animation in end.
TEST_F(AnimationTest, DeleteFromEnd) {
  base::RunLoop loop;
  DeletingAnimationDelegate delegate;
  delegate.set_quit_closure(loop.QuitWhenIdleClosure());
  RunAnimation* animation = new RunAnimation(150, &delegate);
  animation->Start();
  loop.Run();
  // delegate should have deleted animation.
}

TEST_F(AnimationTest, ShouldRenderRichAnimation) {
#if BUILDFLAG(IS_WIN)
  BOOL result;
  ASSERT_NE(0,
            ::SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &result, 0));
  // ShouldRenderRichAnimation() should check the SPI_GETCLIENTAREAANIMATION
  // value on Vista.
  EXPECT_EQ(!!result, Animation::ShouldRenderRichAnimation());
#else
  EXPECT_TRUE(Animation::ShouldRenderRichAnimation());
#endif
}

// Test that current value is always 0 after Start() is called.
TEST_F(AnimationTest, StartState) {
  LinearAnimation animation(base::Milliseconds(100), 60, NULL);
  EXPECT_EQ(0.0, animation.GetCurrentValue());
  animation.Start();
  EXPECT_EQ(0.0, animation.GetCurrentValue());
  animation.End();
  EXPECT_EQ(1.0, animation.GetCurrentValue());
  animation.Start();
  EXPECT_EQ(0.0, animation.GetCurrentValue());
}

///////////////////////////////////////////////////////////////////////////////
// PrefersReducedMotion tests

TEST_F(AnimationTest, PrefersReducedMotionRespectsOverrideFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForcePrefersReducedMotion, "1");
  EXPECT_TRUE(Animation::PrefersReducedMotion());

  // It doesn't matter what the system setting says; the flag should continue to
  // override it.
  Animation::SetPrefersReducedMotionForTesting(false);
  EXPECT_TRUE(Animation::PrefersReducedMotion());
}

}  // namespace gfx
