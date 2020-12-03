// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "testing/platform_test.h"
#import "ui/base/test/cocoa_helper.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

namespace ui {
namespace {

typedef CocoaTest GrabWindowSnapshotTest;

// TODO(https://crbug.com/685088): This test fails.
TEST_F(GrabWindowSnapshotTest, DISABLED_TestGrabWindowSnapshot) {
  // Launch a test window so we can take a snapshot.
  NSRect frame = NSMakeRect(0, 0, 400, 400);
  NSWindow* window = test_window();
  [window setFrame:frame display:false];
  [window setBackgroundColor:[NSColor whiteColor]];
  [window makeKeyAndOrderFront:NSApp];
  [window display];

  gfx::Image image;
  gfx::Rect bounds = gfx::Rect(0, 0, frame.size.width, frame.size.height);
  EXPECT_TRUE(ui::GrabWindowSnapshot(window, bounds, &image));

  NSImage* nsImage = image.ToNSImage();
  CGImageRef cgImage =
      [nsImage CGImageForProposedRect:nil context:nil hints:nil];
  base::scoped_nsobject<NSBitmapImageRep> rep(
      [[NSBitmapImageRep alloc] initWithCGImage:cgImage]);
  EXPECT_TRUE([rep isKindOfClass:[NSBitmapImageRep class]]);
  CGFloat scaleFactor = 1.0f;
  if ([window respondsToSelector:@selector(backingScaleFactor)])
    scaleFactor = [window backingScaleFactor];
  EXPECT_EQ(400 * scaleFactor, CGImageGetWidth([rep CGImage]));
  NSColor* color = [rep colorAtX:200 * scaleFactor y:200 * scaleFactor];
  CGFloat red = 0, green = 0, blue = 0, alpha = 0;
  [color getRed:&red green:&green blue:&blue alpha:&alpha];
  EXPECT_GE(red + green + blue, 3.0);
}

}  // namespace
}  // namespace ui
