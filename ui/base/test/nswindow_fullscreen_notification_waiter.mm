// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/nswindow_fullscreen_notification_waiter.h"

#import "base/mac/sdk_forward_declarations.h"

@interface NSWindowFullscreenNotificationWaiter ()
// Exit the RunLoop if there is one and the counts being tracked match.
- (void)maybeQuitForChangedArg:(int*)changedArg;
- (void)onEnter:(NSNotification*)notification;
- (void)onExit:(NSNotification*)notification;
@end

@implementation NSWindowFullscreenNotificationWaiter

@synthesize enterCount = enterCount_;
@synthesize exitCount = exitCount_;

- (instancetype)initWithWindow:(NSWindow*)window {
  if ((self = [super init])) {
    window_.reset([window retain]);
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(onEnter:)
                          name:NSWindowDidEnterFullScreenNotification
                        object:window];
    [defaultCenter addObserver:self
                      selector:@selector(onExit:)
                          name:NSWindowDidExitFullScreenNotification
                        object:window];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!runLoop_);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)waitForEnterCount:(int)enterCount exitCount:(int)exitCount {
  if (enterCount_ >= enterCount && exitCount_ >= exitCount)
    return;

  targetEnterCount_ = enterCount;
  targetExitCount_ = exitCount;
  runLoop_ = std::make_unique<base::RunLoop>();
  runLoop_->Run();
  runLoop_.reset();
}

- (void)maybeQuitForChangedArg:(int*)changedArg {
  ++*changedArg;
  if (!runLoop_)
    return;

  if (enterCount_ >= targetEnterCount_ && exitCount_ >= targetExitCount_)
    runLoop_->Quit();
}

- (void)onEnter:(NSNotification*)notification {
  [self maybeQuitForChangedArg:&enterCount_];
}

- (void)onExit:(NSNotification*)notification {
  [self maybeQuitForChangedArg:&exitCount_];
}

@end
