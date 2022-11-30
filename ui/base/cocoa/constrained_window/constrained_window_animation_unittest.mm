// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/constrained_window/constrained_window_animation.h"

#include <memory>

#include "base/mac/scoped_nsobject.h"
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
  if (_frameCount >= 2)
    [animation setDuration:0.0];
  return _frameCount == 1 ? 0.2 : 0.6;
}

- (void)animationDidEnd:(NSAnimation*)animation {
  EXPECT_EQ(2, _frameCount);
}

- (void)runAnimation:(NSAnimation*)animation {
  // This class will end the animation after 2 frames. Set a large duration to
  // ensure that both frames are processed.
  [animation setDuration:600];
  [animation setDelegate:self];
  [animation startAnimation];
  EXPECT_EQ(2, _frameCount);
}

@end

class ConstrainedWindowAnimationTest : public ui::CocoaTest {
 protected:
  ConstrainedWindowAnimationTest() : CocoaTest() {
    delegate_.reset([[ConstrainedWindowAnimationTestDelegate alloc] init]);
  }

  base::scoped_nsobject<ConstrainedWindowAnimationTestDelegate> delegate_;
};

// Test the show animation.
TEST_F(ConstrainedWindowAnimationTest, Show) {
  base::scoped_nsobject<NSAnimation> animation(
      [[ConstrainedWindowAnimationShow alloc] initWithWindow:test_window()]);
  [delegate_ runAnimation:animation];
}

// Test the hide animation.
TEST_F(ConstrainedWindowAnimationTest, Hide) {
  base::scoped_nsobject<NSAnimation> animation(
      [[ConstrainedWindowAnimationHide alloc] initWithWindow:test_window()]);
  [delegate_ runAnimation:animation];
}

// Test the pulse animation.
TEST_F(ConstrainedWindowAnimationTest, Pulse) {
  base::scoped_nsobject<NSAnimation> animation(
      [[ConstrainedWindowAnimationPulse alloc] initWithWindow:test_window()]);
  [delegate_ runAnimation:animation];
}
