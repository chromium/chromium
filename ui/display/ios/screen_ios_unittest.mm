// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <memory>

#include "base/test/task_environment.h"
#include "build/ios_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace display {
namespace {

using ScreenIosTest = PlatformTest;

// Regression test for crbug.com/446106888: a notification observer registered
// by `ScreenIos` can outlive `ScreenIos` if destroyed while
// `NSNotificationCenter` is dispatching a notification (retaining or
// autoreleasing the observer across the dispatch). Destroying `ScreenIos` must
// not leave a dangling `raw_ptr` in the observer.
TEST_F(ScreenIosTest, ObserverOutlivingScreenDoesNotDangle) {
  // Intentionally omit `base::test::TaskEnvironment` so the notification
  // handler returns early without posting a task, isolating the observer's
  // lifetime from the posted-task lifetime tested below.
  @autoreleasepool {
    __block std::unique_ptr<Screen> screen(CreateNativeScreen());
    id<NSObject> observer = [NSNotificationCenter.defaultCenter
        addObserverForName:UIWindowDidBecomeKeyNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification*) {
                  screen.reset();
                }];
    [NSNotificationCenter.defaultCenter
        postNotificationName:UIWindowDidBecomeKeyNotification
                      object:nil];
    EXPECT_FALSE(screen);
    [NSNotificationCenter.defaultCenter removeObserver:observer];
  }
}

// Regression test for crbug.com/446106888: `ScreenIos`'s notification handler
// posts an asynchronous `ScreenChanged` task when a default task runner exists.
// Destroying `ScreenIos` before that task runs must not cause a use-after-free,
// and `~ScreenIos()` must unregister its notification observers.
TEST_F(ScreenIosTest, DestroyedBeforePostedScreenChangedRuns) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<Screen> screen(CreateNativeScreen());
  @autoreleasepool {
    [NSNotificationCenter.defaultCenter
        postNotificationName:UIWindowDidBecomeKeyNotification
                      object:nil];
    EXPECT_EQ(task_environment.GetPendingMainThreadTaskCount(), 1u);
#if !BUILDFLAG(IS_IOS_TVOS)
    [NSNotificationCenter.defaultCenter
        postNotificationName:UIDeviceOrientationDidChangeNotification
                      object:nil];
    EXPECT_EQ(task_environment.GetPendingMainThreadTaskCount(), 2u);
#endif
  }

  screen.reset();
  task_environment.FastForwardUntilNoTasksRemain();

  // Verify `~ScreenIos()` unregistered its observers (no new tasks posted).
  [NSNotificationCenter.defaultCenter
      postNotificationName:UIWindowDidBecomeKeyNotification
                    object:nil];
#if !BUILDFLAG(IS_IOS_TVOS)
  [NSNotificationCenter.defaultCenter
      postNotificationName:UIDeviceOrientationDidChangeNotification
                    object:nil];
#endif
  EXPECT_EQ(task_environment.GetPendingMainThreadTaskCount(), 0u);
}

}  // namespace
}  // namespace display
