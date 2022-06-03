// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/nswindow_fullscreen_notification_waiter.h"

@interface NSWindowFullscreenNotificationWaiter ()
// Exit the RunLoop if there is one and the counts being tracked match.
- (void)maybeQuitForChangedArg:(int*)changedArg;
- (void)onEnter:(NSNotification*)notification;
- (void)onExit:(NSNotification*)notification;
@end

@implementation NSWindowFullscreenNotificationWaiter

@synthesize enterCount = _enterCount;
@synthesize exitCount = _exitCount;

- (instancetype)initWithWindow:(NSWindow*)window {
  if ((self = [super init])) {
    _window.reset([window retain]);
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
  DCHECK(!_runLoop);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)waitForEnterCount:(int)enterCount exitCount:(int)exitCount {
  if (_enterCount >= enterCount && _exitCount >= exitCount)
    return;

  _targetEnterCount = enterCount;
  _targetExitCount = exitCount;
  _runLoop = std::make_unique<base::RunLoop>();
  _runLoop->Run();
  _runLoop.reset();
}

- (void)maybeQuitForChangedArg:(int*)changedArg {
  ++*changedArg;
  if (!_runLoop)
    return;

  if (_enterCount >= _targetEnterCount && _exitCount >= _targetExitCount)
    _runLoop->Quit();
}

- (void)onEnter:(NSNotification*)notification {
  [self maybeQuitForChangedArg:&_enterCount];
}

- (void)onExit:(NSNotification*)notification {
  [self maybeQuitForChangedArg:&_exitCount];
}

@end
