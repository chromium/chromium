// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/windowed_nsnotification_observer.h"

#import <Cocoa/Cocoa.h>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"

@interface WindowedNSNotificationObserver ()
- (void)onNotification:(NSNotification*)notification;
@end

@implementation WindowedNSNotificationObserver

@synthesize notificationCount = notificationCount_;

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
    bundleId_.reset([bundleId copy]);
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserver:self
           selector:@selector(onNotification:)
               name:name
             object:nil];
  }
  return self;
}

- (void)dealloc {
  if (bundleId_)
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
  else
    [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)onNotification:(NSNotification*)notification {
  if (bundleId_) {
    NSRunningApplication* application =
        [notification userInfo][NSWorkspaceApplicationKey];
    if (![[application bundleIdentifier] isEqualToString:bundleId_])
      return;
  }

  ++notificationCount_;
  if (runLoop_)
    runLoop_->Quit();
}

- (BOOL)waitForCount:(int)minimumCount {
  while (notificationCount_ < minimumCount) {
    const int oldCount = notificationCount_;
    base::RunLoop runLoop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, runLoop.QuitClosure(), TestTimeouts::action_timeout());
    runLoop_ = &runLoop;
    runLoop.Run();
    runLoop_ = nullptr;

    // If there was no new notification, it must have been a timeout.
    if (notificationCount_ == oldCount)
      break;
  }
  return notificationCount_ >= minimumCount;
}

- (BOOL)wait {
  return [self waitForCount:1];
}

@end
