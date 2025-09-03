// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/test/run_until.h"
#import "testing/gtest_mac.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/events/test/cocoa_test_event_utils.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/native/native_view_host_mac.h"
#include "ui/views/controls/native/native_view_host_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Helper view to track mouse press events.
class TestOverlayView : public View {
  METADATA_HEADER(TestOverlayView, View)
 public:
  TestOverlayView() = default;
  ~TestOverlayView() override = default;

  // View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    mouse_pressed_ = true;
    last_press_location_ = event.location();
    return true;
  }

  void ResetState() {
    mouse_pressed_ = false;
    last_press_location_ = gfx::Point();
  }

  bool mouse_pressed() const { return mouse_pressed_; }
  const gfx::Point& last_press_location() const { return last_press_location_; }

 private:
  bool mouse_pressed_ = false;
  gfx::Point last_press_location_;
};

BEGIN_METADATA(TestOverlayView)
END_METADATA

}  // namespace

class NativeViewHostMacInteractiveTest : public test::NativeViewHostTestBase {
 public:
  NativeViewHostMacInteractiveTest() = default;

  NativeViewHostMacInteractiveTest(const NativeViewHostMacInteractiveTest&) =
      delete;
  NativeViewHostMacInteractiveTest& operator=(
      const NativeViewHostMacInteractiveTest&) = delete;

  void SetUp() override {
    SetUpForInteractiveTests();
    NativeViewHostTestBase::SetUp();
  }

  void TearDown() override {
    // On Mac, the Widget is the host, so it must be closed before the
    // ContextFactory is torn down by ViewsTestBase.
    DestroyTopLevel();
    NativeViewHostTestBase::TearDown();
  }

  NativeViewHostMac* native_host() {
    return static_cast<NativeViewHostMac*>(GetNativeWrapper());
  }

  void CreateHost() {
    CreateTopLevel();
    CreateTestingHost();
    native_view_ = [[NSView alloc] initWithFrame:NSZeroRect];

    EXPECT_FALSE(native_host());

    toplevel()->GetClientContentsView()->AddChildViewRaw(host());
    EXPECT_TRUE(native_host());

    host()->Attach(gfx::NativeView(native_view_));
  }

 protected:
  NSView* __strong native_view_;
};

// Tests event routing by sending NSEvent directly to the NSWindow,
// verifying that mouse up and down events are correctly routed to an overlay
// view or to the NativeViewHost.
TEST_F(NativeViewHostMacInteractiveTest, NSWindowEventDispatchWithOverlay) {
  // Creates toplevel_ widget, host_ NativeViewHost, and attaches native_view_.
  CreateHost();

  NSWindow* ns_window = toplevel()->GetNativeWindow().GetNativeNSWindow();
  ASSERT_TRUE(ns_window);

  // Make the window visible for event processing.
  toplevel()->Show();

  // Give the window non-empty size. The origin does not matter.
  const gfx::Rect toplevel_bounds = gfx::Rect(50, 50, 200, 200);
  toplevel()->SetBounds(toplevel_bounds);
  // remote_cocoa updates the NSWindow asychrnously.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return toplevel()->GetWindowBoundsInScreen().size() ==
           toplevel_bounds.size();
  }));
  gfx::Size root_view_size = toplevel()->GetRootView()->size();

  auto overlay_view_ptr = std::make_unique<TestOverlayView>();
  TestOverlayView* overlay_view =
      toplevel()->GetClientContentsView()->AddChildView(
          std::move(overlay_view_ptr));

  // Make sure the overlay view and the NativeViewHost have same parent, so they
  // are in the same coordinate.
  ASSERT_EQ(overlay_view->parent(), host()->parent());
  // Make sure the parent view and the root view aligns on their origins, so
  // that the event point can be calculated using root view's size.
  ASSERT_EQ(toplevel()->GetRootView()->GetBoundsInScreen().origin(),
            host()->parent()->GetBoundsInScreen().origin());

  host()->SetBounds(0, 0, 100, 100);
  overlay_view->SetBounds(50, 50, 50, 50);
  test::RunScheduledLayout(toplevel());

  // Test 1: Click on the overlay view.

  // Point in RootView coordinates (top-down).
  gfx::Point point_on_overlay(60, 60);
  ASSERT_EQ(overlay_view, toplevel()->GetRootView()->GetEventHandlerForPoint(
                              point_on_overlay));

  // Convert to NSWindow coordinates (bottom-up).
  NSPoint point_on_overlay_ns = NSMakePoint(
      point_on_overlay.x(), root_view_size.height() - point_on_overlay.y());

  [ns_window sendEvent:cocoa_test_event_utils::MouseEventAtPointInWindow(
                           point_on_overlay_ns, NSEventTypeLeftMouseDown,
                           ns_window, 1)];
  // Sending mouse up is important. It resets the mouse event handler in
  // RootView. Without it, a following mouse event will be sent to the same view
  // even if the mouse has moved away from the view.
  [ns_window
      sendEvent:cocoa_test_event_utils::MouseEventAtPointInWindow(
                    point_on_overlay_ns, NSEventTypeLeftMouseUp, ns_window, 1)];

  // The overlay view should be clicked.
  EXPECT_TRUE(overlay_view->mouse_pressed());
  EXPECT_EQ(overlay_view->last_press_location(),
            point_on_overlay - overlay_view->bounds().OffsetFromOrigin());
  EXPECT_EQ(on_mouse_pressed_called_count(), 0);

  // Test 2: Click on NativeViewHost (not covered by overlay).
  overlay_view->ResetState();

  // Point in RootView coordinates (top-down).
  gfx::Point point_on_nvh(20, 20);
  ASSERT_EQ(host(),
            toplevel()->GetRootView()->GetEventHandlerForPoint(point_on_nvh));

  // Convert to NSWindow coordinates (bottom-up).
  NSPoint point_on_nvh_ns =
      NSMakePoint(point_on_nvh.x(), root_view_size.height() - point_on_nvh.y());

  [ns_window
      sendEvent:cocoa_test_event_utils::MouseEventAtPointInWindow(
                    point_on_nvh_ns, NSEventTypeLeftMouseDown, ns_window, 1)];

  // The NativeViewHost should be clicked.
  EXPECT_FALSE(overlay_view->mouse_pressed());
  EXPECT_EQ(on_mouse_pressed_called_count(), 1);

  DestroyHost();
}

}  // namespace views
