// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/mac_util.h"
#include "base/run_loop.h"
#import "ui/base/cocoa/nswindow_test_util.h"
#include "ui/base/hit_test.h"
#include "ui/base/test/ui_controls.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/window/native_frame_view.h"

namespace views::test {

class BridgedNativeWidgetUITest : public WidgetTest {
 public:
  BridgedNativeWidgetUITest() = default;

  BridgedNativeWidgetUITest(const BridgedNativeWidgetUITest&) = delete;
  BridgedNativeWidgetUITest& operator=(const BridgedNativeWidgetUITest&) =
      delete;

  // testing::Test:
  void SetUp() override {
    SetUpForInteractiveTests();
    WidgetTest::SetUp();

    widget_delegate_ = std::make_unique<WidgetDelegate>();

    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    init_params.bounds = gfx::Rect(100, 100, 300, 200);
    init_params.delegate = widget_delegate_.get();

    // Provide a resizable Widget by default, as macOS doesn't correctly restore
    // the window size when coming out of fullscreen if the window is not
    // user-sizable.
    init_params.delegate->SetCanResize(true);

    widget_ = std::make_unique<Widget>();
    widget_->Init(std::move(init_params));
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
  std::unique_ptr<WidgetDelegate> widget_delegate_;
  std::unique_ptr<Widget> widget_;
};

// Tests for correct fullscreen tracking, regardless of whether it is initiated
// by the Widget code or elsewhere (e.g. by the user).
TEST_F(BridgedNativeWidgetUITest, FullscreenSynchronousState) {
  EXPECT_FALSE(widget_->IsFullscreen());

  // Allow user-initiated fullscreen changes on the Window.
  [test_window()
      setCollectionBehavior:[test_window() collectionBehavior] |
                            NSWindowCollectionBehaviorFullScreenPrimary];

  ui::NSWindowFullscreenNotificationWaiter waiter(widget_->GetNativeWindow());
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
  // toggling above. Wait only for the exit notification (the enter
  // notification will be swallowed, because the exit will have been requested
  // before the enter completes).
  waiter.WaitForEnterAndExitCount(0, 1);
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
}

// Test fullscreen without overlapping calls and without changing collection
// behavior on the test window.
TEST_F(BridgedNativeWidgetUITest, FullscreenEnterAndExit) {
  ui::NSWindowFullscreenNotificationWaiter waiter(widget_->GetNativeWindow());

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
  EXPECT_EQ(0, waiter.enter_count());
  waiter.WaitForEnterAndExitCount(1, 0);

  // Verify it hasn't exceeded.
  EXPECT_EQ(1, waiter.enter_count());
  EXPECT_EQ(0, waiter.exit_count());
  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  widget_->SetFullscreen(false);
  EXPECT_FALSE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  waiter.WaitForEnterAndExitCount(1, 1);
  EXPECT_EQ(1, waiter.enter_count());
  EXPECT_EQ(1, waiter.exit_count());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
}

// Test that Widget::Restore exits fullscreen.
TEST_F(BridgedNativeWidgetUITest, FullscreenRestore) {
  ui::NSWindowFullscreenNotificationWaiter waiter(widget_->GetNativeWindow());

  EXPECT_FALSE(widget_->IsFullscreen());
  const gfx::Rect restored_bounds = widget_->GetRestoredBounds();
  EXPECT_FALSE(restored_bounds.IsEmpty());

  widget_->SetFullscreen(true);
  EXPECT_TRUE(widget_->IsFullscreen());
  waiter.WaitForEnterAndExitCount(1, 0);

  widget_->Restore();
  EXPECT_FALSE(widget_->IsFullscreen());
  waiter.WaitForEnterAndExitCount(1, 1);
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
}

}  // namespace views::test
