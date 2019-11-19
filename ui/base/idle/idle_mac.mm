// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

#include "ui/base/idle/idle_internal.h"

@interface MacScreenMonitor : NSObject {
 @private
  BOOL screensaverRunning_;
  BOOL screenLocked_;
}

@property (readonly,
           nonatomic,
           getter=isScreensaverRunning) BOOL screensaverRunning;
@property (readonly, nonatomic, getter=isScreenLocked) BOOL screenLocked;

@end

@implementation MacScreenMonitor

@synthesize screensaverRunning = screensaverRunning_;
@synthesize screenLocked = screenLocked_;

- (instancetype)init {
  if ((self = [super init])) {
    NSDistributedNotificationCenter* distCenter =
          [NSDistributedNotificationCenter defaultCenter];
    [distCenter addObserver:self
                   selector:@selector(onScreenSaverStarted:)
                       name:@"com.apple.screensaver.didstart"
                     object:nil];
    [distCenter addObserver:self
                   selector:@selector(onScreenSaverStopped:)
                       name:@"com.apple.screensaver.didstop"
                     object:nil];
    [distCenter addObserver:self
                   selector:@selector(onScreenLocked:)
                       name:@"com.apple.screenIsLocked"
                     object:nil];
    [distCenter addObserver:self
                   selector:@selector(onScreenUnlocked:)
                       name:@"com.apple.screenIsUnlocked"
                     object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)onScreenSaverStarted:(NSNotification*)notification {
   screensaverRunning_ = YES;
}

- (void)onScreenSaverStopped:(NSNotification*)notification {
   screensaverRunning_ = NO;
}

- (void)onScreenLocked:(NSNotification*)notification {
   screenLocked_ = YES;
}

- (void)onScreenUnlocked:(NSNotification*)notification {
   screenLocked_ = NO;
}

@end

namespace ui {
namespace {

static MacScreenMonitor* g_screenMonitor = nil;

}  // namespace

void InitIdleMonitor() {
  if (!g_screenMonitor)
    g_screenMonitor = [[MacScreenMonitor alloc] init];
}

int CalculateIdleTime() {
  CFTimeInterval idle_time = CGEventSourceSecondsSinceLastEventType(
      kCGEventSourceStateCombinedSessionState,
      kCGAnyInputEventType);
  return static_cast<int>(idle_time);
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value())
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;

  return [g_screenMonitor isScreensaverRunning] ||
      [g_screenMonitor isScreenLocked];
}

}  // namespace ui
