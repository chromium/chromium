// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/constrained_window/constrained_window_animation.h"

#include <memory>

#import "ui/base/test/cocoa_helper.h"

// This class runs an animation for exactly two frames then end it.
@interface ConstrainedWindowAnimationTestDelegate
    : NSObject<NSAnimationDelegate> {
 @private
  CGFloat _frameCount;
}

- (void)runAnimation:(NSAnimation*)animation;

@end

@implementation ConstrainedWindowAnimationTestDelegate

- (float)animation:(NSAnimation*)animation
    valueForProgress:(NSAnimationProgress)progress {
  ++_frameCount;
  if (_frameCount >= 2) {
    animation.duration = 0.0;
  }
  return _frameCount == 1 ? 0.2 : 0.6;
}

- (void)animationDidEnd:(NSAnimation*)animation {
  EXPECT_EQ(2, _frameCount);
}

- (void)runAnimation:(NSAnimation*)animation {
  // This class will end the animation after 2 frames. Set a large duration to
  // ensure that both frames are processed.
  animation.duration = 600;
  animation.delegate = self;
  [animation startAnimation];
  EXPECT_EQ(2, _frameCount);
}

@end

class ConstrainedWindowAnimationTest : public ui::CocoaTest {
 protected:
  ConstrainedWindowAnimationTest() {
    delegate_ = [[ConstrainedWindowAnimationTestDelegate alloc] init];
  }

  ConstrainedWindowAnimationTestDelegate* __strong delegate_;
};

// Test the show animation.
TEST_F(ConstrainedWindowAnimationTest, Show) {
  NSAnimation* animation =
      [[ConstrainedWindowAnimationShow alloc] initWithWindow:test_window()];
  [delegate_ runAnimation:animation];
}

// Test the hide animation.
TEST_F(ConstrainedWindowAnimationTest, Hide) {
  NSAnimation* animation =
      [[ConstrainedWindowAnimationHide alloc] initWithWindow:test_window()];
  [delegate_ runAnimation:animation];
}

// Test the pulse animation.
TEST_F(ConstrainedWindowAnimationTest, Pulse) {
  NSAnimation* animation =
      [[ConstrainedWindowAnimationPulse alloc] initWithWindow:test_window()];
  [delegate_ runAnimation:animation];
}
