// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/scoped_fake_nswindow_fullscreen.h"

#import "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#import "ui/gfx/mac/nswindow_frame_controls.h"

@interface TestNSWindowDelegate : NSObject<NSWindowDelegate> {
 @private
  NSSize targetSize_;
}
- (instancetype)initWithFullScreenContentSize:(NSSize)targetSize;
@end

@implementation TestNSWindowDelegate

- (instancetype)initWithFullScreenContentSize:(NSSize)targetSize {
  if ((self = [super init])) {
    targetSize_ = targetSize;
  }
  return self;
}

- (NSSize)window:(NSWindow*)window
    willUseFullScreenContentSize:(NSSize)proposedSize {
  return targetSize_;
}

@end

namespace ui {
namespace test {

// Test the order of notifications sent when faking fullscreen transitions.
TEST(ScopedFakeNSWindowFullscreenTest, TestOrdering) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI);

  NSUInteger style_mask = NSTexturedBackgroundWindowMask | NSTitledWindowMask |
                          NSClosableWindowMask | NSMiniaturizableWindowMask |
                          NSResizableWindowMask;
  base::scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:NSMakeRect(50, 60, 130, 170)
                                  styleMask:style_mask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  [window setReleasedWhenClosed:NO];
  [window orderFront:nil];
  NSRect initial_frame = [window frame];

  NSSize fullscreen_content_size = NSMakeSize(500, 400);
  base::scoped_nsobject<TestNSWindowDelegate> delegate(
      [[TestNSWindowDelegate alloc]
          initWithFullScreenContentSize:fullscreen_content_size]);
  [window setDelegate:delegate];

  base::scoped_nsobject<WindowedNSNotificationObserver> will_enter(
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowWillEnterFullScreenNotification
                       object:window]);
  base::scoped_nsobject<WindowedNSNotificationObserver> did_enter(
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidEnterFullScreenNotification
                       object:window]);
  base::scoped_nsobject<WindowedNSNotificationObserver> will_exit(
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowWillExitFullScreenNotification
                       object:window]);
  base::scoped_nsobject<WindowedNSNotificationObserver> did_exit(
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidExitFullScreenNotification
                       object:window]);

  test::ScopedFakeNSWindowFullscreen fake_fullscreen;

  // Nothing happens if the window cannot go fullscreen.
  gfx::SetNSWindowCanFullscreen(window, false);
  [window toggleFullScreen:nil];
  EXPECT_EQ(0, [will_enter notificationCount]);

  gfx::SetNSWindowCanFullscreen(window, true);

  // WillEnter is immediate.
  [window toggleFullScreen:nil];
  EXPECT_EQ(1, [will_enter notificationCount]);
  EXPECT_EQ(0, [did_enter notificationCount]);
  EXPECT_NSEQ(initial_frame, [window frame]);
  EXPECT_FALSE([window styleMask] & NSFullScreenWindowMask);

  // Changes and DidEnter happen asynchronously.
  EXPECT_TRUE([did_enter wait]);
  EXPECT_EQ(fullscreen_content_size.width, [window frame].size.width);
  EXPECT_EQ(fullscreen_content_size.height, [window frame].size.height);
  EXPECT_TRUE([window styleMask] & NSFullScreenWindowMask);

  // WillExit is immediate.
  [window toggleFullScreen:nil];
  EXPECT_EQ(1, [will_exit notificationCount]);
  EXPECT_EQ(0, [did_exit notificationCount]);
  EXPECT_EQ(fullscreen_content_size.width, [window frame].size.width);
  EXPECT_EQ(fullscreen_content_size.height, [window frame].size.height);
  EXPECT_TRUE([window styleMask] & NSFullScreenWindowMask);

  // Changes and DidExit happen asynchronously.
  EXPECT_TRUE([did_exit wait]);
  EXPECT_NSEQ(initial_frame, [window frame]);
  EXPECT_FALSE([window styleMask] & NSFullScreenWindowMask);

  // Go back into fullscreen.
  [window toggleFullScreen:nil];
  EXPECT_TRUE([did_enter waitForCount:2]);

  // On the way out, call -[NSWindow setFrame:]. It should stay at those bounds.
  NSRect new_frame = NSMakeRect(90, 90, 90, 90);

  // -[NSWindow frameRectForContentRect:] uses some internal state that isn't
  // faked, so grab the expected final size before starting the transition. Then
  // set the new frame during the transition and, once the transition completes,
  // ensure the expected size wasn't clobbered by the faker,
  NSRect frame_outside_fullscreen = [window frameRectForContentRect:new_frame];
  [window toggleFullScreen:nil];
  [window setFrame:new_frame display:YES animate:NO];
  EXPECT_TRUE([did_exit waitForCount:2]);
  EXPECT_NSEQ(frame_outside_fullscreen, [window frame]);

  [window close];
}

}  // namespace test
}  // namespace ui
