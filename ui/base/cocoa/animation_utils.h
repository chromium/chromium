// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_ANIMATION_UTILS_H_
#define UI_BASE_COCOA_ANIMATION_UTILS_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#import <Cocoa/Cocoa.h>
#endif

#import <QuartzCore/QuartzCore.h>

// This class is a stack-based helper useful for unit testing of Cocoa UI,
// and any other situation where you want to temporarily turn off Cocoa
// animation for the life of a function call or other limited scope.
// Just declare one of these, and all animations will complete instantly until
// this goes out of scope and pops our state off the Core Animation stack.
//
// Example:
//  MyUnitTest() {
//    WithNoAnimation at_all; // Turn off Cocoa auto animation in this scope.

#if BUILDFLAG(IS_MAC)
class WithNoAnimation {
 public:
  WithNoAnimation() {
    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:0.0];
  }

  ~WithNoAnimation() {
   [NSAnimationContext endGrouping];
  }
};
#endif

// Disables actions within a scope.
class ScopedCAActionDisabler {
 public:
  ScopedCAActionDisabler() {
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithBool:YES]
                     forKey:kCATransactionDisableActions];
  }

  ~ScopedCAActionDisabler() {
    [CATransaction commit];
  }
};

// Sets a duration on actions within a scope.
class ScopedCAActionSetDuration {
 public:
  explicit ScopedCAActionSetDuration(NSTimeInterval duration) {
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithFloat:duration]
                     forKey:kCATransactionAnimationDuration];
  }

  ~ScopedCAActionSetDuration() {
    [CATransaction commit];
  }
};

#endif  // UI_BASE_COCOA_ANIMATION_UTILS_H_
