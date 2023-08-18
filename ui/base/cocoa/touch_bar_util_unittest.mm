// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/touch_bar_util.h"

#include "base/apple/foundation_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/base/test/cocoa_helper.h"

namespace {

const char kTestChromeBundleId[] = "test.bundleid";

NSString* const kTestTouchBarId = @"test-touch-bar";

NSString* const kTestTouchBarItemId = @"TEST-ITEM";

}  // namespace

class TouchBarUtilTest : public ui::CocoaTest {
 public:
  TouchBarUtilTest() {}
};

TEST_F(TouchBarUtilTest, TouchBarIdentifiers) {
  base::apple::SetBaseBundleID(kTestChromeBundleId);
  EXPECT_TRUE([ui::GetTouchBarId(kTestTouchBarId)
      isEqualToString:@"test.bundleid.test-touch-bar"]);
  EXPECT_TRUE([ui::GetTouchBarItemId(kTestTouchBarId, kTestTouchBarItemId)
      isEqualToString:@"test.bundleid.test-touch-bar-TEST-ITEM"]);
}
