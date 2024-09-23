// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view.h"

#import <Cocoa/Cocoa.h>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

enum SwipeType {
  SWIPE_NONE,
  SWIPE_LEFT,
  SWIPE_RIGHT,
  SWIPE_UP,
  SWIPE_DOWN,
};

// Stores last received swipe gesture direction.
class ThreeFingerSwipeView : public View {
  METADATA_HEADER(ThreeFingerSwipeView, View)

 public:
  // View:
  void OnGestureEvent(ui::GestureEvent* event) override {
    EXPECT_EQ(ui::EventType::kGestureSwipe, event->details().type());

    if (event->details().swipe_left()) {
      last_swipe_ = SWIPE_LEFT;
    } else if (event->details().swipe_right()) {
      last_swipe_ = SWIPE_RIGHT;
    } else if (event->details().swipe_up()) {
      last_swipe_ = SWIPE_UP;
    } else if (event->details().swipe_down()) {
      last_swipe_ = SWIPE_DOWN;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  SwipeType last_swipe() const { return last_swipe_; }

 private:
  SwipeType last_swipe_{SWIPE_NONE};
};

BEGIN_METADATA(ThreeFingerSwipeView)
END_METADATA

using ViewMacTest = test::WidgetTest;

SwipeType SendSwipeGesture(ThreeFingerSwipeView* view, NSPoint gesture) {
  NSWindow* window = view->GetWidget()->GetNativeWindow().GetNativeNSWindow();
  FakeSwipeEvent* swipe_event = [[FakeSwipeEvent alloc] init];
  swipe_event.deltaX = gesture.x;
  swipe_event.deltaY = gesture.y;
  swipe_event.window = window;
  swipe_event.locationInWindow = NSMakePoint(50, 50);
  swipe_event.timestamp = NSProcessInfo.processInfo.systemUptime;

  // BridgedContentView should create an appropriate ui::GestureEvent and pass
  // it to the Widget.
  [window.contentView swipeWithEvent:swipe_event];
  return view->last_swipe();
}

// Test that three-finger swipe events are translated by BridgedContentView.
TEST_F(ViewMacTest, HandlesThreeFingerSwipeGestures) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  widget->SetBounds(gfx::Rect(0, 0, 100, 100));
  widget->Show();

  auto* view = widget->non_client_view()->frame_view()->AddChildView(
      std::make_unique<ThreeFingerSwipeView>());
  view->SetSize(widget->GetClientAreaBoundsInScreen().size());

  // Remember that in AppKit coordinates, x and y increase towards the top-left.
  const NSPoint nsleft = NSMakePoint(1, 0);
  const NSPoint nsright = NSMakePoint(-1, 0);
  const NSPoint nsup = NSMakePoint(0, 1);
  const NSPoint nsdown = NSMakePoint(0, -1);

  EXPECT_EQ(SWIPE_LEFT, SendSwipeGesture(view, nsleft));
  EXPECT_EQ(SWIPE_RIGHT, SendSwipeGesture(view, nsright));
  EXPECT_EQ(SWIPE_DOWN, SendSwipeGesture(view, nsdown));
  EXPECT_EQ(SWIPE_UP, SendSwipeGesture(view, nsup));
}

}  // namespace

}  // namespace views
