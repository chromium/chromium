// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/mac_util.h"
#include "base/test/task_environment.h"
#include "testing/platform_test.h"
#import "ui/base/test/cocoa_helper.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ui {
namespace {

class GrabWindowSnapshotTest : public CocoaTest {
 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

TEST_F(GrabWindowSnapshotTest, TestGrabWindowSnapshot) {
  // Flaky only on the 10.13 bot yet not on any subsequent macOS bot.
  // https://crbug.com/1359153
  if (base::mac::IsOS10_13())
    GTEST_SKIP() << "flaky on macOS 10.13 bot";

  // The window snapshot code uses `CGWindowListCreateImage` which requires
  // going to the windowserver. By default, unittests are run with the
  // `NSApplicationActivationPolicyProhibited` policy which prohibits
  // windowserver connections, which would cause this test to fail for reasons
  // other than the code not actually working.
  NSApp.activationPolicy = NSApplicationActivationPolicyAccessory;

  // Launch a test window so we can take a snapshot.
  const NSUInteger window_size = 400;
  NSRect frame = NSMakeRect(0, 0, window_size, window_size);
  NSWindow* window = test_window();
  WindowedNSNotificationObserver* waiter =
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidUpdateNotification
                       object:window];
  [window setFrame:frame display:false];
  window.backgroundColor = NSColor.blueColor;
  [window makeKeyAndOrderFront:nil];
  [window display];
  EXPECT_TRUE([waiter wait]);

  // Take the snapshot.
  gfx::Image image;
  gfx::Rect bounds = gfx::Rect(0, 0, window_size, window_size);
  EXPECT_TRUE(ui::GrabWindowSnapshot(window, bounds, &image));

  // The call to `CGWindowListCreateImage` returned a `CGImageRef` that is
  // wrapped in an `NSImage` (inside the returned `gfx::Image`). The image rep
  // that results (e.g. an `NSCGImageSnapshotRep` in macOS 12) isn't anything
  // that pixel values can be retrieved from, so do a quick-and-dirty conversion
  // to an `NSBitmapImageRep`.
  NSBitmapImageRep* image_rep =
      [NSBitmapImageRep imageRepWithData:image.ToNSImage().TIFFRepresentation];

  // Test the size.
  EXPECT_EQ(window_size * window.backingScaleFactor, image_rep.pixelsWide);
  EXPECT_EQ(window_size * window.backingScaleFactor, image_rep.pixelsHigh);

  // Pick a pixel in the middle of the screenshot and expect it to be some
  // version of blue.
  NSColor* color = [image_rep colorAtX:image_rep.pixelsWide / 2
                                     y:image_rep.pixelsHigh / 2];
  CGFloat red = 0, green = 0, blue = 0, alpha = 0;
  [color getRed:&red green:&green blue:&blue alpha:&alpha];
  EXPECT_LE(red, 0.2);
  EXPECT_LE(green, 0.2);
  EXPECT_GE(blue, 0.9);
  EXPECT_EQ(alpha, 1);
}

}  // namespace
}  // namespace ui
