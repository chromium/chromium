// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/windowed_nsnotification_observer.h"

#import <Cocoa/Cocoa.h>

#include "base/location.h"
#include "base/run_loop.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"

@interface WindowedNSNotificationObserver ()
- (void)onNotification:(NSNotification*)notification;
@end

@implementation WindowedNSNotificationObserver

@synthesize notificationCount = _notificationCount;

- (instancetype)initForNotification:(NSString*)name {
  return [self initForNotification:name object:nil];
}

- (instancetype)initForNotification:(NSString*)name object:(id)sender {
  if ((self = [super init])) {
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(onNotification:)
                                                 name:name
                                               object:sender];
  }
  return self;
}

- (instancetype)initForWorkspaceNotification:(NSString*)name
                                    bundleId:(NSString*)bundleId {
  if ((self = [super init])) {
    _bundleId.reset([bundleId copy]);
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserver:self
           selector:@selector(onNotification:)
               name:name
             object:nil];
  }
  return self;
}

- (void)dealloc {
  if (_bundleId)
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
  else
    [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)onNotification:(NSNotification*)notification {
  if (_bundleId) {
    NSRunningApplication* application =
        [notification userInfo][NSWorkspaceApplicationKey];
    if (![[application bundleIdentifier] isEqualToString:_bundleId])
      return;
  }

  ++_notificationCount;
  if (_runLoop)
    _runLoop->Quit();
}

- (BOOL)waitForCount:(int)minimumCount {
  while (_notificationCount < minimumCount) {
    const int oldCount = _notificationCount;
    base::RunLoop runLoop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, runLoop.QuitClosure(), TestTimeouts::action_timeout());
    _runLoop = &runLoop;
    runLoop.Run();
    _runLoop = nullptr;

    // If there was no new notification, it must have been a timeout.
    if (_notificationCount == oldCount)
      break;
  }
  return _notificationCount >= minimumCount;
}

- (BOOL)wait {
  return [self waitForCount:1];
}

@end
