// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views_bridge_mac/bridged_native_widget_impl.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "ui/base/hit_test.h"
#import "ui/base/test/nswindow_fullscreen_notification_waiter.h"
#include "ui/base/test/ui_controls.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/views/cocoa/bridged_native_widget_host_impl.h"
#include "ui/views/test/views_interactive_ui_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/window/native_frame_view.h"

namespace views {
namespace test {
namespace {

// Provide a resizable Widget by default. Starting in 10.11, OSX doesn't
// correctly restore the window size when coming out of fullscreen if the window
// is not user-sizable.
class ResizableDelegateView : public WidgetDelegateView {
 public:
  ResizableDelegateView() {}

  // WidgetDelgate:
  bool CanResize() const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ResizableDelegateView);
};

}  // namespace

class BridgedNativeWidgetUITest : public test::WidgetTest {
 public:
  BridgedNativeWidgetUITest() = default;

  // testing::Test:
  void SetUp() override {
    ViewsInteractiveUITestBase::InteractiveSetUp();
    WidgetTest::SetUp();
    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    init_params.bounds = gfx::Rect(100, 100, 300, 200);
    init_params.delegate = new ResizableDelegateView;
    widget_.reset(new Widget);
    widget_->Init(init_params);
  }

  void TearDown() override {
    // Ensures any compositor is removed before ViewsTestBase tears down the
    // ContextFactory.
    widget_.reset();
    WidgetTest::TearDown();
  }

  NSWindow* test_window() {
    return widget_->GetNativeWindow().GetNativeNSWindow();
  }

 protected:
  std::unique_ptr<Widget> widget_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidgetUITest);
};

// Tests for correct fullscreen tracking, regardless of whether it is initiated
// by the Widget code or elsewhere (e.g. by the user).
TEST_F(BridgedNativeWidgetUITest, FullscreenSynchronousState) {
  EXPECT_FALSE(widget_->IsFullscreen());

  // Allow user-initiated fullscreen changes on the Window.
  [test_window()
      setCollectionBehavior:[test_window() collectionBehavior] |
                            NSWindowCollectionBehaviorFullScreenPrimary];

  base::scoped_nsobject<NSWindowFullscreenNotificationWaiter> waiter(
      [[NSWindowFullscreenNotificationWaiter alloc]
          initWithWindow:test_window()]);
  const gfx::Rect restored_bounds = widget_->GetRestoredBounds();

  // First show the widget. A user shouldn't be able to initiate fullscreen
  // unless the window is visible in the first place.
  widget_->Show();

  // Simulate a user-initiated fullscreen. Note trying to to this again before
  // spinning a runloop will cause Cocoa to emit text to stdio and ignore it.
  [test_window() toggleFullScreen:nil];
  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  // Note there's now an animation running. While that's happening, toggling the
  // state should work as expected, but do "nothing".
  widget_->SetFullscreen(false);
  EXPECT_FALSE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
  widget_->SetFullscreen(false);  // Same request - should no-op.
  EXPECT_FALSE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  widget_->SetFullscreen(true);
  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  // Always finish out of fullscreen. Otherwise there are 4 NSWindow objects
  // that Cocoa creates which don't close themselves and will be seen by the Mac
  // test harness on teardown. Note that the test harness will be waiting until
  // all animations complete, since these temporary animation windows will not
  // be removed from the window list until they do.
  widget_->SetFullscreen(false);
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  // Now we must wait for the notifications. Since, if the widget is torn down,
  // the NSWindowDelegate is removed, and the pending request to take out of
  // fullscreen is lost. Since a message loop has not yet spun up in this test
  // we can reliably say there will be one enter and one exit, despite all the
  // toggling above.
  [waiter waitForEnterCount:1 exitCount:1];
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
}

// Test fullscreen without overlapping calls and without changing collection
// behavior on the test window.
TEST_F(BridgedNativeWidgetUITest, FullscreenEnterAndExit) {
  base::scoped_nsobject<NSWindowFullscreenNotificationWaiter> waiter(
      [[NSWindowFullscreenNotificationWaiter alloc]
          initWithWindow:test_window()]);

  EXPECT_FALSE(widget_->IsFullscreen());
  const gfx::Rect restored_bounds = widget_->GetRestoredBounds();
  EXPECT_FALSE(restored_bounds.IsEmpty());

  // Ensure this works without having to change collection behavior as for the
  // test above. Also check that making a hidden widget fullscreen shows it.
  EXPECT_FALSE(widget_->IsVisible());
  widget_->SetFullscreen(true);
  EXPECT_TRUE(widget_->IsVisible());

  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  // Should be zero until the runloop spins.
  EXPECT_EQ(0, [waiter enterCount]);
  [waiter waitForEnterCount:1 exitCount:0];

  // Verify it hasn't exceeded.
  EXPECT_EQ(1, [waiter enterCount]);
  EXPECT_EQ(0, [waiter exitCount]);
  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  widget_->SetFullscreen(false);
  EXPECT_FALSE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  [waiter waitForEnterCount:1 exitCount:1];
  EXPECT_EQ(1, [waiter enterCount]);
  EXPECT_EQ(1, [waiter exitCount]);
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
}

// Test that Widget::Restore exits fullscreen.
TEST_F(BridgedNativeWidgetUITest, FullscreenRestore) {
  base::scoped_nsobject<NSWindowFullscreenNotificationWaiter> waiter(
      [[NSWindowFullscreenNotificationWaiter alloc]
          initWithWindow:test_window()]);

  EXPECT_FALSE(widget_->IsFullscreen());
  const gfx::Rect restored_bounds = widget_->GetRestoredBounds();
  EXPECT_FALSE(restored_bounds.IsEmpty());

  widget_->SetFullscreen(true);
  EXPECT_TRUE(widget_->IsFullscreen());
  [waiter waitForEnterCount:1 exitCount:0];

  widget_->Restore();
  EXPECT_FALSE(widget_->IsFullscreen());
  [waiter waitForEnterCount:1 exitCount:1];
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
}

namespace {

// This is used to return a customized result to NonClientHitTest.
class HitTestNonClientFrameView : public NativeFrameView {
 public:
  explicit HitTestNonClientFrameView(Widget* widget)
      : NativeFrameView(widget), hit_test_result_(HTNOWHERE) {}

  // NonClientFrameView overrides:
  int NonClientHitTest(const gfx::Point& point) override {
    return hit_test_result_;
  }

  void set_hit_test_result(int component) { hit_test_result_ = component; }

 private:
  int hit_test_result_;

  DISALLOW_COPY_AND_ASSIGN(HitTestNonClientFrameView);
};

// This is used to change whether the Widget is resizable.
class HitTestWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit HitTestWidgetDelegate(views::Widget* widget) : widget_(widget) {}

  void set_can_resize(bool can_resize) {
    can_resize_ = can_resize;
    widget_->OnSizeConstraintsChanged();
  }

  // views::WidgetDelegate:
  bool CanResize() const override { return can_resize_; }
  views::Widget* GetWidget() override { return widget_; }
  views::Widget* GetWidget() const override { return widget_; }

 private:
  views::Widget* widget_;
  bool can_resize_ = false;

  DISALLOW_COPY_AND_ASSIGN(HitTestWidgetDelegate);
};

void WaitForEvent(NSUInteger mask) {
  // Pointer because the handler block captures local variables by copying.
  base::RunLoop run_loop;
  base::RunLoop* run_loop_ref = &run_loop;
  id monitor = [NSEvent
      addLocalMonitorForEventsMatchingMask:mask
                                   handler:^NSEvent*(NSEvent* ns_event) {
                                     run_loop_ref->Quit();
                                     return ns_event;
                                   }];
  run_loop.Run();
  [NSEvent removeMonitor:monitor];
}

}  // namespace

// This is used to inject test versions of NativeFrameView and
// BridgedNativeWidgetImpl.
class HitTestNativeWidgetMac : public NativeWidgetMac {
 public:
  HitTestNativeWidgetMac(internal::NativeWidgetDelegate* delegate,
                         NativeFrameView* native_frame_view)
      : NativeWidgetMac(delegate), native_frame_view_(native_frame_view) {
    bridge_host_ = std::make_unique<BridgedNativeWidgetHostImpl>(this);
  }

  // internal::NativeWidgetPrivate:
  NonClientFrameView* CreateNonClientFrameView() override {
    return native_frame_view_;
  }

 private:
  // Owned by Widget.
  NativeFrameView* native_frame_view_;

  DISALLOW_COPY_AND_ASSIGN(HitTestNativeWidgetMac);
};

// Flaky on macOS 10.12. See http://crbug.com/767299.
TEST_F(BridgedNativeWidgetUITest, DISABLED_HitTest) {
  Widget widget;
  HitTestNonClientFrameView* frame_view =
      new HitTestNonClientFrameView(&widget);
  test::HitTestNativeWidgetMac* native_widget =
      new test::HitTestNativeWidgetMac(&widget, frame_view);
  HitTestWidgetDelegate* widget_delegate = new HitTestWidgetDelegate(&widget);
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.native_widget = native_widget;
  init_params.delegate = widget_delegate;
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.bounds = gfx::Rect(100, 200, 400, 300);
  widget.Init(init_params);

  WidgetActivationWaiter activation_waiter(&widget, true);
  widget.Show();
  activation_waiter.Wait();

  // Points inside the resize area.
  const NSPoint bottom_right_point = {398, 2};
  const NSPoint right_of_bottom_right = {398 + 10, 2};

  NSWindow* window = widget.GetNativeWindow().GetNativeNSWindow();

  EXPECT_FALSE([window ignoresMouseEvents]);
  // OSX uses both the alpha value of the window and the underlying CALayer to
  // decide whether to send mouse events to window, in case [window
  // ignoresMouseEvent] is not explicitly initialized. Since, no frames are
  // drawn during tests and the underlying CALayer has a transparent background,
  // explicitly call setIgnoresMouseEvents: to ensure the window receives the
  // mouse events.
  [window setIgnoresMouseEvents:NO];

  // Dragging the window should work.
  frame_view->set_hit_test_result(HTCAPTION);
  {
    EXPECT_EQ(100, [window frame].origin.x);

    base::scoped_nsobject<WindowedNSNotificationObserver> will_move_observer(
        [[WindowedNSNotificationObserver alloc]
            initForNotification:NSWindowWillMoveNotification]);
    NSEvent* mouse_down = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
        NSMakePoint(20, 20), window);
    CGEventPost(kCGSessionEventTap, [mouse_down CGEvent]);
    WaitForEvent(NSLeftMouseDownMask);

    base::scoped_nsobject<WindowedNSNotificationObserver> did_move_observer(
        [[WindowedNSNotificationObserver alloc]
            initForNotification:NSWindowDidMoveNotification]);
    NSEvent* mouse_drag = cocoa_test_event_utils::MouseEventAtPointInWindow(
        NSMakePoint(30, 30), NSLeftMouseDragged, window, 0);
    CGEventPost(kCGSessionEventTap, [mouse_drag CGEvent]);
    WaitForEvent(NSLeftMouseDraggedMask);
    // NSWindowWillMoveNotification should have been observed by the time the
    // mouse drag event is received.
    EXPECT_EQ(1, [will_move_observer notificationCount]);
    EXPECT_TRUE([did_move_observer wait]);
    EXPECT_EQ(110, [window frame].origin.x);

    NSEvent* mouse_up = cocoa_test_event_utils::MouseEventAtPointInWindow(
        NSMakePoint(20, 20), NSLeftMouseUp, window, 0);
    CGEventPost(kCGSessionEventTap, [mouse_up CGEvent]);
    WaitForEvent(NSLeftMouseUpMask);
    EXPECT_EQ(110, [window frame].origin.x);
  }

  // Dragging in the resize area works since the widget is not resizable.
  {
    EXPECT_EQ(110, [window frame].origin.x);

    base::scoped_nsobject<WindowedNSNotificationObserver> will_move_observer(
        [[WindowedNSNotificationObserver alloc]
            initForNotification:NSWindowWillMoveNotification]);
    NSEvent* mouse_down = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
        bottom_right_point, window);
    CGEventPost(kCGSessionEventTap, [mouse_down CGEvent]);
    WaitForEvent(NSLeftMouseDownMask);

    base::scoped_nsobject<WindowedNSNotificationObserver> did_move_observer(
        [[WindowedNSNotificationObserver alloc]
            initForNotification:NSWindowDidMoveNotification]);
    NSEvent* mouse_drag = cocoa_test_event_utils::MouseEventAtPointInWindow(
        right_of_bottom_right, NSLeftMouseDragged, window, 0);
    CGEventPost(kCGSessionEventTap, [mouse_drag CGEvent]);
    WaitForEvent(NSLeftMouseDraggedMask);
    EXPECT_EQ(1, [will_move_observer notificationCount]);
    EXPECT_TRUE([did_move_observer wait]);
    EXPECT_EQ(120, [window frame].origin.x);

    NSEvent* mouse_up = cocoa_test_event_utils::MouseEventAtPointInWindow(
        bottom_right_point, NSLeftMouseUp, window, 0);
    CGEventPost(kCGSessionEventTap, [mouse_up CGEvent]);
    WaitForEvent(NSLeftMouseUpMask);
    EXPECT_EQ(120, [window frame].origin.x);
  }

  // If the widget is resizable, dragging in the resize area should not repost
  // (and should resize).
  widget_delegate->set_can_resize(true);
  {
    EXPECT_EQ(400, [window frame].size.width);

    NSUInteger x = [window frame].origin.x;
    NSUInteger y = [window frame].origin.y;

    // Enqueue all mouse events first because AppKit will run its own loop to
    // consume them.
    base::scoped_nsobject<WindowedNSNotificationObserver> will_move_observer(
        [[WindowedNSNotificationObserver alloc]
            initForNotification:NSWindowWillMoveNotification]);
    NSEvent* mouse_down = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
        bottom_right_point, window);
    CGEventPost(kCGSessionEventTap, [mouse_down CGEvent]);

    base::scoped_nsobject<WindowedNSNotificationObserver> did_resize_observer(
        [[WindowedNSNotificationObserver alloc]
            initForNotification:NSWindowDidResizeNotification]);
    NSEvent* mouse_drag = cocoa_test_event_utils::MouseEventAtPoint(
        NSMakePoint(x + 408, y + 2), NSLeftMouseDragged, 0);
    CGEventPost(kCGSessionEventTap, [mouse_drag CGEvent]);

    NSEvent* mouse_up = cocoa_test_event_utils::MouseEventAtPoint(
        NSMakePoint(x + 408, y + 2), NSLeftMouseUp, 0);
    CGEventPost(kCGSessionEventTap, [mouse_up CGEvent]);

    EXPECT_TRUE([did_resize_observer wait]);
    EXPECT_EQ(0, [will_move_observer notificationCount]);
    EXPECT_EQ(410, [window frame].size.width);

    // Origin is unchanged.
    EXPECT_EQ(x, [window frame].origin.x);
    EXPECT_EQ(y, [window frame].origin.y);
  }

  // Mouse-downs on the window controls should not be intercepted.
  {
    EXPECT_EQ(120, [window frame].origin.x);

    base::scoped_nsobject<WindowedNSNotificationObserver> will_move_observer(
        [[WindowedNSNotificationObserver alloc]
            initForNotification:NSWindowWillMoveNotification]);
    base::scoped_nsobject<WindowedNSNotificationObserver>
        did_miniaturize_observer([[WindowedNSNotificationObserver alloc]
            initForNotification:NSWindowDidMiniaturizeNotification]);

    // Position this on the minimize button.
    NSEvent* mouse_down = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
        NSMakePoint(30, 290), window);
    CGEventPost(kCGSessionEventTap, [mouse_down CGEvent]);

    NSEvent* mouse_up = cocoa_test_event_utils::MouseEventAtPointInWindow(
        NSMakePoint(30, 290), NSLeftMouseUp, window, 0);
    EXPECT_FALSE([window isMiniaturized]);
    CGEventPost(kCGSessionEventTap, [mouse_up CGEvent]);
    [did_miniaturize_observer wait];
    EXPECT_EQ(0, [will_move_observer notificationCount]);
    EXPECT_TRUE([window isMiniaturized]);
    [window deminiaturize:nil];

    // Position unchanged.
    EXPECT_EQ(120, [window frame].origin.x);
  }

  // Non-draggable areas should do nothing.
  frame_view->set_hit_test_result(HTCLIENT);
  {
    EXPECT_EQ(120, [window frame].origin.x);

    base::scoped_nsobject<WindowedNSNotificationObserver> will_move_observer(
        [[WindowedNSNotificationObserver alloc]
            initForNotification:NSWindowWillMoveNotification]);
    NSEvent* mouse_down = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
        NSMakePoint(20, 20), window);
    CGEventPost(kCGSessionEventTap, [mouse_down CGEvent]);
    WaitForEvent(NSLeftMouseDownMask);

    NSEvent* mouse_drag = cocoa_test_event_utils::MouseEventAtPointInWindow(
        NSMakePoint(30, 30), NSLeftMouseDragged, window, 0);
    CGEventPost(kCGSessionEventTap, [mouse_drag CGEvent]);
    WaitForEvent(NSLeftMouseDraggedMask);
    EXPECT_EQ(0, [will_move_observer notificationCount]);
    EXPECT_EQ(120, [window frame].origin.x);

    NSEvent* mouse_up = cocoa_test_event_utils::MouseEventAtPointInWindow(
        NSMakePoint(30, 30), NSLeftMouseUp, window, 0);
    CGEventPost(kCGSessionEventTap, [mouse_up CGEvent]);
    WaitForEvent(NSLeftMouseUpMask);
    EXPECT_EQ(120, [window frame].origin.x);
  }
}

}  // namespace test
}  // namespace views
