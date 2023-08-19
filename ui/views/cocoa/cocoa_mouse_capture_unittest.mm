// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/mouse_capture.h"

#import <Cocoa/Cocoa.h>

#import "components/remote_cocoa/app_shim/mouse_capture_delegate.h"
#import "ui/base/test/cocoa_helper.h"
#import "ui/events/test/cocoa_test_event_utils.h"

// Simple test view that counts calls to -[NSView mouseDown:].
@interface CocoaMouseCaptureTestView : NSView
@property(readonly, nonatomic) int mouseDownCount;
@end

@implementation CocoaMouseCaptureTestView {
  int _mouseDownCount;
}

@synthesize mouseDownCount = _mouseDownCount;

- (void)mouseDown:(NSEvent*)theEvent {
  ++_mouseDownCount;
}

@end

namespace remote_cocoa {
namespace {

// Simple capture delegate that just counts events forwarded.
class TestCaptureDelegate : public CocoaMouseCaptureDelegate {
 public:
  explicit TestCaptureDelegate(NSWindow* window) : window_(window) {}

  TestCaptureDelegate(const TestCaptureDelegate&) = delete;
  TestCaptureDelegate& operator=(const TestCaptureDelegate&) = delete;

  void Acquire() { mouse_capture_ = std::make_unique<CocoaMouseCapture>(this); }
  bool IsActive() { return mouse_capture_ && mouse_capture_->IsActive(); }
  void SimulateDestroy() { mouse_capture_.reset(); }
  void set_should_claim_event(bool should_claim_event) {
    should_claim_event_ = should_claim_event;
  }

  int event_count() { return event_count_; }
  int capture_lost_count() { return capture_lost_count_; }

  // CocoaMouseCaptureDelegate:
  bool PostCapturedEvent(NSEvent* event) override {
    ++event_count_;
    return should_claim_event_;
  }
  void OnMouseCaptureLost() override { ++capture_lost_count_; }
  NSWindow* GetWindow() const override { return window_; }

 private:
  std::unique_ptr<CocoaMouseCapture> mouse_capture_;
  bool should_claim_event_ = true;
  int event_count_ = 0;
  int capture_lost_count_ = 0;
  NSWindow* __strong window_;
};

}  // namespace

using CocoaMouseCaptureTest = ui::CocoaTest;

// Test that a new capture properly "steals" capture from an existing one.
TEST_F(CocoaMouseCaptureTest, OnCaptureLost) {
  TestCaptureDelegate capture(nil);

  capture.Acquire();
  EXPECT_TRUE(capture.IsActive());
  {
    TestCaptureDelegate capture2(nil);
    EXPECT_EQ(0, capture.capture_lost_count());

    // A second capture steals from the first.
    capture2.Acquire();
    EXPECT_TRUE(capture2.IsActive());
    EXPECT_FALSE(capture.IsActive());
    EXPECT_EQ(1, capture.capture_lost_count());
    EXPECT_EQ(0, capture2.capture_lost_count());

    // Simulate capture2 going out of scope. Inspect it.
    capture2.SimulateDestroy();
    EXPECT_FALSE(capture2.IsActive());
    EXPECT_EQ(1, capture2.capture_lost_count());
  }

  // Re-acquiring is fine (not stealing).
  EXPECT_FALSE(capture.IsActive());
  capture.Acquire();
  EXPECT_TRUE(capture.IsActive());

  // Having no CocoaMouseCapture instance is fine.
  capture.SimulateDestroy();
  EXPECT_FALSE(capture.IsActive());
  // Receives OnMouseCaptureLost again, since reacquired.
  EXPECT_EQ(2, capture.capture_lost_count());
}

// Test event capture.
TEST_F(CocoaMouseCaptureTest, CaptureEvents) {
  CocoaMouseCaptureTestView* view =
      [[CocoaMouseCaptureTestView alloc] initWithFrame:NSZeroRect];
  test_window().contentView = view;
  NSArray<NSEvent*>* click = cocoa_test_event_utils::MouseClickInView(view, 1);

  // First check that the view receives events normally.
  EXPECT_EQ(0, view.mouseDownCount);
  [NSApp sendEvent:click[0]];
  EXPECT_EQ(1, view.mouseDownCount);

  {
    TestCaptureDelegate capture(test_window());
    capture.Acquire();

    // Now check that the capture captures events.
    EXPECT_EQ(0, capture.event_count());
    [NSApp sendEvent:click[0]];
    EXPECT_EQ(1, view.mouseDownCount);
    EXPECT_EQ(1, capture.event_count());
  }

  // After the capture goes away, events should be received again.
  [NSApp sendEvent:click[0]];
  EXPECT_EQ(2, view.mouseDownCount);
}

// Test local events properly swallowed / propagated.
TEST_F(CocoaMouseCaptureTest, SwallowOrPropagateEvents) {
  CocoaMouseCaptureTestView* view =
      [[CocoaMouseCaptureTestView alloc] initWithFrame:NSZeroRect];
  test_window().contentView = view;
  NSArray<NSEvent*>* click = cocoa_test_event_utils::MouseClickInView(view, 1);

  // First check that the view receives events normally.
  EXPECT_EQ(0, view.mouseDownCount);
  [NSApp sendEvent:click[0]];
  EXPECT_EQ(1, view.mouseDownCount);

  {
    TestCaptureDelegate capture(test_window());
    capture.Acquire();

    // By default, the delegate should claim events.
    EXPECT_EQ(0, capture.event_count());
    [NSApp sendEvent:click[0]];
    EXPECT_EQ(1, view.mouseDownCount);
    EXPECT_EQ(1, capture.event_count());

    // Set the delegate not to claim events.
    capture.set_should_claim_event(false);
    [NSApp sendEvent:click[0]];
    EXPECT_EQ(2, view.mouseDownCount);
    EXPECT_EQ(2, capture.event_count());

    // Setting it back should restart the claiming of events.
    capture.set_should_claim_event(true);
    [NSApp sendEvent:click[0]];
    EXPECT_EQ(2, view.mouseDownCount);
    EXPECT_EQ(3, capture.event_count());
  }
}

}  // namespace remote_cocoa
