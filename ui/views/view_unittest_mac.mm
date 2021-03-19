// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/optional.h"
#include "ui/events/gesture_event_details.h"
#include "ui/views/test/widget_test.h"

// We can't create NSEventTypeSwipe using normal means, and rely on duck typing
// instead.
@interface FakeSwipeEvent : NSEvent
@property CGFloat deltaX;
@property CGFloat deltaY;
@property(assign) NSWindow* window;
@property NSPoint locationInWindow;
@property NSEventModifierFlags modifierFlags;
@property NSTimeInterval timestamp;
@end

@implementation FakeSwipeEvent
@synthesize deltaX;
@synthesize deltaY;
@synthesize window;
@synthesize locationInWindow;
@synthesize modifierFlags;
@synthesize timestamp;

- (NSEventType)type {
  return NSEventTypeSwipe;
}

- (NSEventSubtype)subtype {
  // themblsha: In my testing, all native three-finger NSEventTypeSwipe events
  // all had 0 as their subtype.
  return static_cast<NSEventSubtype>(0);
}
@end

namespace views {

namespace {

// Stores last received swipe gesture direction vector in
// |last_swipe_gesture()|.
class ThreeFingerSwipeView : public View {
 public:
  ThreeFingerSwipeView() = default;

  // View:
  void OnGestureEvent(ui::GestureEvent* event) override {
    EXPECT_EQ(ui::ET_GESTURE_SWIPE, event->details().type());

    int dx = 0, dy = 0;
    if (event->details().swipe_left())
      dx = -1;

    if (event->details().swipe_right()) {
      EXPECT_EQ(0, dx);
      dx = 1;
    }

    if (event->details().swipe_down())
      dy = 1;

    if (event->details().swipe_up()) {
      EXPECT_EQ(0, dy);
      dy = -1;
    }

    last_swipe_gesture_ = gfx::Point(dx, dy);
  }

  base::Optional<gfx::Point> last_swipe_gesture() const {
    return last_swipe_gesture_;
  }

 private:
  base::Optional<gfx::Point> last_swipe_gesture_;

  DISALLOW_COPY_AND_ASSIGN(ThreeFingerSwipeView);
};

}  // namespace

class ViewMacTest : public test::WidgetTest {
 public:
  ViewMacTest() = default;

  base::Optional<gfx::Point> SwipeGestureVector(int dx, int dy) {
    base::scoped_nsobject<FakeSwipeEvent> swipe_event(
        [[FakeSwipeEvent alloc] init]);
    [swipe_event setDeltaX:dx];
    [swipe_event setDeltaY:dy];
    [swipe_event setWindow:widget_->GetNativeWindow().GetNativeNSWindow()];
    [swipe_event setLocationInWindow:NSMakePoint(50, 50)];
    [swipe_event setTimestamp:[[NSProcessInfo processInfo] systemUptime]];

    // BridgedContentView should create an appropriate ui::GestureEvent and pass
    // it to the Widget.
    [[widget_->GetNativeWindow().GetNativeNSWindow() contentView]
        swipeWithEvent:swipe_event];
    return view_->last_swipe_gesture();
  }

  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();

    widget_ = CreateTopLevelPlatformWidget();
    widget_->SetBounds(gfx::Rect(0, 0, 100, 100));
    widget_->Show();

    view_ = new ThreeFingerSwipeView;
    view_->SetSize(widget_->GetClientAreaBoundsInScreen().size());
    widget_->GetContentsView()->AddChildView(view_);
  }

  void TearDown() override {
    widget_->CloseNow();
    WidgetTest::TearDown();
  }

 private:
  Widget* widget_ = nullptr;
  ThreeFingerSwipeView* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ViewMacTest);
};

// Three-finger swipes send immediate events and they cannot be tracked.
TEST_F(ViewMacTest, HandlesThreeFingerSwipeGestures) {
  // Note that positive delta is left and up for NSEvent, which is the inverse
  // of ui::GestureEventDetails.
  EXPECT_EQ(gfx::Point(1, 0), *SwipeGestureVector(-1, 0));
  EXPECT_EQ(gfx::Point(-1, 0), *SwipeGestureVector(1, 0));
  EXPECT_EQ(gfx::Point(0, 1), *SwipeGestureVector(0, -1));
  EXPECT_EQ(gfx::Point(0, -1), *SwipeGestureVector(0, 1));
}

}  // namespace views
