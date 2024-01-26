// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_WINDOWED_NSNOTIFICATION_OBSERVER_H_
#define UI_BASE_TEST_WINDOWED_NSNOTIFICATION_OBSERVER_H_

#import <Foundation/Foundation.h>

namespace base {
class RunLoop;
}

// This class starts watching for an NSNotification upon construction and can
// wait until the notification is observed. This guarantees that notifications
// fired between calls to -init and -wait will be caught.
@interface WindowedNSNotificationObserver : NSObject

@property(readonly, nonatomic) int notificationCount;

// Watch for an NSNotification on the default notification center for the given
// |notificationSender|, or a nil object if omitted.
- (instancetype)initForNotification:(NSString*)name;

// Watch for an NSNotification on the default notification center from a
// particular object.
- (instancetype)initForNotification:(NSString*)name object:(id)sender;

// Watch for an NSNotification on the shared workspace notification center for
// the given application.
- (instancetype)initForWorkspaceNotification:(NSString*)name
                                    bundleId:(NSString*)bundleId;

// Waits for |minimumCount| notifications to be observed and returns YES.
// Returns NO on a timeout. This can be called multiple times.
- (BOOL)waitForCount:(int)minimumCount;

// Returns YES if any notification has been observed, or spins a RunLoop until
// it either is observed, or a timeout occurs. Returns NO on a timeout.
- (BOOL)wait;
@end

#endif  // UI_BASE_TEST_WINDOWED_NSNOTIFICATION_OBSERVER_H_
