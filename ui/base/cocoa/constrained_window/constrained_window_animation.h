// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_ANIMATION_H_
#define UI_BASE_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_ANIMATION_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "ui/base/ui_base_export.h"

// Base class for all constrained window animation classes.
UI_BASE_EXPORT
@interface ConstrainedWindowAnimationBase : NSAnimation {
 @protected
  base::scoped_nsobject<NSWindow> window_;
}

- (instancetype)initWithWindow:(NSWindow*)window;

@end

// An animation to show a window.
UI_BASE_EXPORT
@interface ConstrainedWindowAnimationShow : ConstrainedWindowAnimationBase
@end

// An animation to hide a window.
UI_BASE_EXPORT
@interface ConstrainedWindowAnimationHide : ConstrainedWindowAnimationBase
@end

// An animation that pulses the window by growing it then shrinking it back.
UI_BASE_EXPORT
@interface ConstrainedWindowAnimationPulse : ConstrainedWindowAnimationBase
@end

#endif  // UI_BASE_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_ANIMATION_H_
