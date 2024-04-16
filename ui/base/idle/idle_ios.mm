// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#import <UIKit/UIKit.h>

#include "base/notreached.h"
#include "ui/base/idle/idle_internal.h"

@interface IOSScreenMonitor : NSObject

@property(readonly, nonatomic, getter=isAppInBackground) BOOL appInBackground;
@property(readonly, nonatomic, getter=isDeviceLocked) BOOL deviceLocked;

@end

@implementation IOSScreenMonitor

@synthesize appInBackground = _appInBackground;
@synthesize deviceLocked = _deviceLocked;

- (instancetype)init {
  if ((self = [super init])) {
    NSNotificationCenter* defaultCenter = NSNotificationCenter.defaultCenter;
    [defaultCenter addObserver:self
                      selector:@selector(onAppDidEnterBackground:)
                          name:UIApplicationDidEnterBackgroundNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(onAppWillEnterForeground:)
                          name:UIApplicationWillEnterForegroundNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(onDeviceLocked:)
                          name:UIApplicationProtectedDataWillBecomeUnavailable
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(onDeviceUnlocked:)
                          name:UIApplicationProtectedDataDidBecomeAvailable
                        object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)onAppDidEnterBackground:(NSNotification*)notification {
  _appInBackground = YES;
}

- (void)onAppWillEnterForeground:(NSNotification*)notification {
  _appInBackground = NO;
}

- (void)onDeviceLocked:(NSNotification*)notification {
  _deviceLocked = YES;
}

- (void)onDeviceUnlocked:(NSNotification*)notification {
  _deviceLocked = NO;
}

@end

namespace ui {
namespace {

static IOSScreenMonitor* g_screenMonitor = nil;

}  // namespace

void InitIdleMonitor() {
  if (!g_screenMonitor) {
    g_screenMonitor = [[IOSScreenMonitor alloc] init];
  }
}

int CalculateIdleTime() {
  // TODO(crbug.com/40255110): Implement this.
  NOTIMPLEMENTED();
  return 0;
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value()) {
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;
  }

  return g_screenMonitor.appInBackground || g_screenMonitor.deviceLocked;
}

}  // namespace ui
