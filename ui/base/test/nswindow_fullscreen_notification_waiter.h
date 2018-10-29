// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_NSWINDOW_FULLSCREEN_NOTIFICATION_WAITER_H_
#define UI_BASE_TEST_NSWINDOW_FULLSCREEN_NOTIFICATION_WAITER_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"

// Waits for fullscreen transitions to complete.
@interface NSWindowFullscreenNotificationWaiter : NSObject {
 @private
  std::unique_ptr<base::RunLoop> runLoop_;
  base::scoped_nsobject<NSWindow> window_;
  int enterCount_;
  int exitCount_;
  int targetEnterCount_;
  int targetExitCount_;
}

@property(readonly, nonatomic) int enterCount;
@property(readonly, nonatomic) int exitCount;

// Initialize for the given window and start tracking notifications.
- (instancetype)initWithWindow:(NSWindow*)window;

// Keep spinning a run loop until the enter and exit counts match.
- (void)waitForEnterCount:(int)enterCount exitCount:(int)exitCount;

@end

#endif  // UI_BASE_TEST_NSWINDOW_FULLSCREEN_NOTIFICATION_WAITER_H_
