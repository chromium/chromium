// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/bubble_closer.h"

#include "base/functional/bind.h"
#import "ui/base/test/cocoa_helper.h"
#import "ui/base/test/menu_test_observer.h"
#import "ui/events/test/cocoa_test_event_utils.h"

namespace ui {

class BubbleCloserTest : public CocoaTest {
 public:
  enum Button { LEFT, RIGHT };
  enum InOrOut { INSIDE, OUTSIDE };

  BubbleCloserTest() = default;

  BubbleCloserTest(const BubbleCloserTest&) = delete;
  BubbleCloserTest& operator=(const BubbleCloserTest&) = delete;

  void SetUp() override {
    CocoaTest::SetUp();
    bubble_window_ =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 320, 200)
                                    styleMask:NSWindowStyleMaskBorderless
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    bubble_window_.releasedWhenClosed = NO;
    [bubble_window_ makeKeyAndOrderFront:nil];
    bubble_closer_ = std::make_unique<BubbleCloser>(
        bubble_window_,
        base::BindRepeating([](int* i) { *i += 1; }, &click_outside_count_));
  }

  void TearDown() override {
    [bubble_window_ close];
    bubble_closer_ = nullptr;
    bubble_window_ = nil;
    CocoaTest::TearDown();
  }

  void SendClick(Button left_or_right, InOrOut in_or_out) {
    NSWindow* window = in_or_out == INSIDE ? bubble_window_ : test_window();
    NSEvent* event =
        left_or_right == LEFT
            ? cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
                  NSMakePoint(10, 10), window)
            : cocoa_test_event_utils::RightMouseDownAtPointInWindow(
                  NSMakePoint(10, 10), window);
    [NSApp sendEvent:event];
  }

  void ResetCloser() { bubble_closer_ = nullptr; }

 protected:
  int click_outside_count() const { return click_outside_count_; }
  NSWindow* bubble_window() { return bubble_window_; }
  bool IsCloserReset() const { return !bubble_closer_; }

 private:
  NSWindow* __strong bubble_window_;
  std::unique_ptr<BubbleCloser> bubble_closer_;
  int click_outside_count_ = 0;
};

// Test for lifetime issues around NSEvent monitors.
TEST_F(BubbleCloserTest, SecondBubbleCloser) {
  auto resetter = [](BubbleCloserTest* me) { me->ResetCloser(); };
  auto deleter = std::make_unique<BubbleCloser>(
      bubble_window(), base::BindRepeating(resetter, this));
  SendClick(LEFT, OUTSIDE);

  // The order is non-deterministic, so click_outside_count() may not change.
  // But either way, the closer should have been reset.
  EXPECT_TRUE(IsCloserReset());
}

// Test that clicking outside the window fires the callback and clicking inside
// does not.
TEST_F(BubbleCloserTest, ClickInsideAndOut) {
  EXPECT_EQ(0, click_outside_count());
  SendClick(LEFT, OUTSIDE);
  EXPECT_EQ(1, click_outside_count());
  SendClick(RIGHT, OUTSIDE);
  EXPECT_EQ(2, click_outside_count());
  SendClick(LEFT, INSIDE);
  EXPECT_EQ(2, click_outside_count());
  SendClick(RIGHT, INSIDE);
  EXPECT_EQ(2, click_outside_count());
}

// Test that right-clicking the window to display a context menu works.
TEST_F(BubbleCloserTest, RightClickOutsideClosesWithContextMenu) {
  NSMenu* context_menu = [[NSMenu alloc] initWithTitle:@""];
  [context_menu addItemWithTitle:@"ContextMenuTest"
                          action:nil
                   keyEquivalent:@""];

  // Set the menu as the contextual menu of contentView of test_window().
  [test_window().contentView setMenu:context_menu];

  MenuTestObserver* menu_observer =
      [[MenuTestObserver alloc] initWithMenu:context_menu];
  menu_observer.closeAfterOpening = YES;
  menu_observer.openCallback = ^(MenuTestObserver* observer) {
    // Verify click is seen when contextual menu is open.
    EXPECT_TRUE(observer.isOpen);
    EXPECT_EQ(1, click_outside_count());
  };

  EXPECT_FALSE(menu_observer.isOpen);
  EXPECT_FALSE(menu_observer.didOpen);

  EXPECT_EQ(0, click_outside_count());

  // RightMouseDown in test_window() would close the bubble window and then
  // display the contextual menu.
  SendClick(RIGHT, OUTSIDE);

  // When we got here, menu has already run its RunLoop.
  EXPECT_EQ(1, click_outside_count());

  EXPECT_FALSE(menu_observer.isOpen);
  EXPECT_TRUE(menu_observer.didOpen);
}

}  // namespace ui
