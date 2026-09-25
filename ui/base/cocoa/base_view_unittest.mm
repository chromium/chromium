// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/base_view.h"

#import <Cocoa/Cocoa.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/base/test/cocoa_helper.h"
#import "ui/events/test/cocoa_test_event_utils.h"

// A BaseView that records the mouse events routed to -mouseEvent:.
@interface MouseEventRecordingView : BaseView
@property(nonatomic) NSUInteger mouseExitedCount;
@end

@implementation MouseEventRecordingView
@synthesize mouseExitedCount = _mouseExitedCount;
- (void)mouseEvent:(NSEvent*)theEvent {
  if (theEvent.type == NSEventTypeMouseExited) {
    self.mouseExitedCount++;
  }
}
@end

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

// Hiding a view while the cursor is over it must synthesize a mouse exited
// event, since AppKit does not send one. See https://crbug.com/548314090.
TEST_F(BaseViewTest, HideViewUnderCursorSynthesizesMouseExit) {
  CocoaTestHelperWindow* window = [[CocoaTestHelperWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 200, 200)];
  window.releasedWhenClosed = NO;
  MouseEventRecordingView* view = [[MouseEventRecordingView alloc]
      initWithFrame:NSMakeRect(0, 0, 100, 100)];
  [window.contentView addSubview:view];

  // Cursor never entered the view: hiding it synthesizes nothing.
  view.hidden = YES;
  EXPECT_EQ(0u, view.mouseExitedCount);
  view.hidden = NO;

  // Cursor inside the view: hiding it synthesizes a mouse exited event.
  [view mouseEntered:cocoa_test_event_utils::EnterEvent(NSMakePoint(50, 50),
                                                        window)];
  view.hidden = YES;
  EXPECT_EQ(1u, view.mouseExitedCount);
  view.hidden = NO;

  // Cursor exited the view again: only the real exit itself is recorded,
  // hiding does not synthesize another one.
  [view mouseExited:cocoa_test_event_utils::ExitEvent(NSMakePoint(150, 150),
                                                      window)];
  EXPECT_EQ(2u, view.mouseExitedCount);
  view.hidden = YES;
  EXPECT_EQ(2u, view.mouseExitedCount);
  view.hidden = NO;

  // A mouse move also marks the cursor as inside, and hiding an ancestor
  // synthesizes a mouse exited event as well.
  [view mouseMoved:cocoa_test_event_utils::MouseEventAtPoint(
                       NSMakePoint(50, 50), NSEventTypeMouseMoved, 0)];
  window.contentView.hidden = YES;
  EXPECT_EQ(3u, view.mouseExitedCount);
  window.contentView.hidden = NO;

  [view removeFromSuperview];
  [window close];
}

}  // namespace
