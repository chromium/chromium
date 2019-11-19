// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/gfx/mac/coordinate_conversion.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/macros.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#include "ui/gfx/geometry/rect.h"

const int kTestWidth = 320;
const int kTestHeight = 200;

// Class to donate an implementation of -[NSScreen frame] that provides a known
// value for robust tests.
@interface MacCoordinateConversionTestScreenDonor : NSObject
- (NSRect)frame;
@end

@implementation MacCoordinateConversionTestScreenDonor
- (NSRect)frame {
  return NSMakeRect(0, 0, kTestWidth, kTestHeight);
}
@end

namespace gfx {
namespace {

class MacCoordinateConversionTest : public PlatformTest {
 public:
  MacCoordinateConversionTest() {}

  // Overridden from testing::Test:
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<base::mac::ScopedObjCClassSwizzler> swizzle_frame_;

  DISALLOW_COPY_AND_ASSIGN(MacCoordinateConversionTest);
};

void MacCoordinateConversionTest::SetUp() {
  // Before swizzling, do a sanity check that the primary screen's origin is
  // (0, 0). This should always be true.
  NSRect primary_screen_frame = [[[NSScreen screens] firstObject] frame];
  EXPECT_EQ(0, primary_screen_frame.origin.x);
  EXPECT_EQ(0, primary_screen_frame.origin.y);

  swizzle_frame_ = std::make_unique<base::mac::ScopedObjCClassSwizzler>(
      [NSScreen class], [MacCoordinateConversionTestScreenDonor class],
      @selector(frame));

  primary_screen_frame = [[[NSScreen screens] firstObject] frame];
  EXPECT_EQ(kTestWidth, primary_screen_frame.size.width);
  EXPECT_EQ(kTestHeight, primary_screen_frame.size.height);
}

void MacCoordinateConversionTest::TearDown() {
  swizzle_frame_.reset();
}

}  // namespace

// Tests for coordinate conversion on Mac. Start with the following setup:
// AppKit ....... gfx
// 199              0
// 189             10   Window of height 40 fills in pixel
// 179  ---------  20   at index 20
// 169  |       |  30   through
// ...  :       :  ..   to
// 150  |       |  49   pixel
// 140  ---------  59   at index 59
// 130             69   (inclusive).
//  ..             ..
//   0            199
TEST_F(MacCoordinateConversionTest, ScreenRectToFromNSRect) {
  // Window on the primary screen.
  Rect gfx_rect = Rect(10, 20, 30, 40);
  NSRect ns_rect = ScreenRectToNSRect(gfx_rect);
  EXPECT_NSEQ(NSMakeRect(10, 140, 30, 40), ns_rect);
  EXPECT_EQ(gfx_rect, ScreenRectFromNSRect(ns_rect));

  // Window in a screen to the left of the primary screen.
  gfx_rect = Rect(-40, 20, 30, 40);
  ns_rect = ScreenRectToNSRect(gfx_rect);
  EXPECT_NSEQ(NSMakeRect(-40, 140, 30, 40), ns_rect);
  EXPECT_EQ(gfx_rect, ScreenRectFromNSRect(ns_rect));

  // Window in a screen below the primary screen.
  gfx_rect = Rect(10, 220, 30, 40);
  ns_rect = ScreenRectToNSRect(gfx_rect);
  EXPECT_NSEQ(NSMakeRect(10, -60, 30, 40), ns_rect);
  EXPECT_EQ(gfx_rect, ScreenRectFromNSRect(ns_rect));

  // Window in a screen below and to the left primary screen.
  gfx_rect = Rect(-40, 220, 30, 40);
  ns_rect = ScreenRectToNSRect(gfx_rect);
  EXPECT_NSEQ(NSMakeRect(-40, -60, 30, 40), ns_rect);
  EXPECT_EQ(gfx_rect, ScreenRectFromNSRect(ns_rect));
}

// Test point conversions using the same setup as ScreenRectToFromNSRect, but
// using only the origin.
TEST_F(MacCoordinateConversionTest, ScreenPointToFromNSPoint) {
  // Point on the primary screen.
  Point gfx_point = Point(10, 20);
  NSPoint ns_point = ScreenPointToNSPoint(gfx_point);
  EXPECT_NSEQ(NSMakePoint(10, 180), ns_point);
  EXPECT_EQ(gfx_point, ScreenPointFromNSPoint(ns_point));

  // Point in a screen to the left of the primary screen.
  gfx_point = Point(-40, 20);
  ns_point = ScreenPointToNSPoint(gfx_point);
  EXPECT_NSEQ(NSMakePoint(-40, 180), ns_point);
  EXPECT_EQ(gfx_point, ScreenPointFromNSPoint(ns_point));

  // Point in a screen below the primary screen.
  gfx_point = Point(10, 220);
  ns_point = ScreenPointToNSPoint(gfx_point);
  EXPECT_NSEQ(NSMakePoint(10, -20), ns_point);
  EXPECT_EQ(gfx_point, ScreenPointFromNSPoint(ns_point));

  // Point in a screen below and to the left primary screen.
  gfx_point = Point(-40, 220);
  ns_point = ScreenPointToNSPoint(gfx_point);
  EXPECT_NSEQ(NSMakePoint(-40, -20), ns_point);
  EXPECT_EQ(gfx_point, ScreenPointFromNSPoint(ns_point));
}

}  // namespace gfx
