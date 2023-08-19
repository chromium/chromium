// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/cocoa/base_view.h"
#import "ui/base/test/cocoa_helper.h"

namespace {

class BaseViewTest : public ui::CocoaTest {
 public:
  BaseViewTest() {
    NSRect frame = NSMakeRect(0, 0, 100, 100);
    BaseView* view = [[BaseView alloc] initWithFrame:frame];
    [test_window().contentView addSubview:view];
    view_ = view;
  }

  BaseView* __weak view_;
};

TEST_F(BaseViewTest, RemoveFromSuperviewWorks) {
  NSView* view = view_;
  EXPECT_EQ(test_window().contentView, view.superview);
  [view removeFromSuperview];
  EXPECT_FALSE(view.superview);
}

// Convert a rect in |view_|'s Cocoa coordinate system to gfx::Rect's top-left
// coordinate system. Repeat the process in reverse and make sure we come out
// with the original rect.
TEST_F(BaseViewTest, flipNSRectToRect) {
  NSRect convert = NSMakeRect(10, 10, 50, 50);
  gfx::Rect converted = [view_ flipNSRectToRect:convert];
  EXPECT_EQ(converted.x(), 10);
  EXPECT_EQ(converted.y(), 40);  // Due to view being 100px tall.
  EXPECT_EQ(converted.width(), NSWidth(convert));
  EXPECT_EQ(converted.height(), NSHeight(convert));

  // Go back the other way.
  NSRect back_again = [view_ flipRectToNSRect:converted];
  EXPECT_EQ(NSMinX(back_again), NSMinX(convert));
  EXPECT_EQ(NSMinY(back_again), NSMinY(convert));
  EXPECT_EQ(NSWidth(back_again), NSWidth(convert));
  EXPECT_EQ(NSHeight(back_again), NSHeight(convert));
}

}  // namespace
