// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/mac_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/platform_test.h"
#import "ui/base/test/cocoa_helper.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/snapshot/snapshot_mac.h"

namespace ui {
namespace {

class GrabWindowSnapshotTest : public CocoaTest,
                               public testing::WithParamInterface<SnapshotAPI> {
 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

INSTANTIATE_TEST_SUITE_P(Snapshot,
                         GrabWindowSnapshotTest,
                         ::testing::Values(SnapshotAPI::kOldAPI,
                                           SnapshotAPI::kNewAPI));

TEST_P(GrabWindowSnapshotTest, TestGrabWindowSnapshot) {
  SnapshotAPI api = GetParam();
  if (api == SnapshotAPI::kNewAPI && base::mac::MacOSVersion() < 14'04'00) {
    GTEST_SKIP() << "Cannot test macOS 14.4 API on pre-14.4 macOS";
  }
  if (api == SnapshotAPI::kNewAPI) {
    GTEST_SKIP() << "https://crbug.com/335449467";
  }

  ForceAPIUsageForTesting(api);

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
  base::test::TestFuture<gfx::Image> future;
  gfx::Rect bounds = gfx::Rect(0, 0, window_size, window_size);
  ui::GrabWindowSnapshot(window, bounds, future.GetCallback());

  gfx::Image image = future.Take();
  ASSERT_TRUE(!image.IsEmpty());
  NSImage* ns_image = image.ToNSImage();

  // Expect the image's size to match the size of the window, scaled
  // appropriately. Expect exactly one representation.
  EXPECT_EQ(window_size * window.backingScaleFactor, ns_image.size.width);
  EXPECT_EQ(window_size * window.backingScaleFactor, ns_image.size.height);
  EXPECT_EQ(1u, ns_image.representations.count);

  // Pick a pixel in the middle of the screenshot and expect it to be some
  // version of blue.
  SkColor color = gfx::test::GetPlatformImageColor(ns_image, window_size / 2,
                                                   window_size / 2);
  EXPECT_LE(SkColorGetR(color), 10u);
  EXPECT_LE(SkColorGetG(color), 10u);
  EXPECT_GE(SkColorGetB(color), 245u);
  EXPECT_EQ(SkColorGetA(color), 255u);

  ForceAPIUsageForTesting(SnapshotAPI::kUnspecified);
}

}  // namespace
}  // namespace ui
