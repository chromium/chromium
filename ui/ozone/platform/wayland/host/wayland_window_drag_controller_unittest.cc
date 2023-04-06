// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input-event-codes.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <xdg-shell-server-protocol.h>

#include <cstdint>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_wayland_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/test/scoped_wl_array.h"
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_util.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_drag_drop_test.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"

using testing::_;
using testing::Mock;
using testing::Values;

namespace ui {

using mojom::DragEventSource;

class WaylandWindowDragControllerTest : public WaylandDragDropTest {
 public:
  WaylandWindowDragControllerTest() = default;
  ~WaylandWindowDragControllerTest() override = default;

  void SetUp() override {
    WaylandDragDropTest::SetUp();
    drag_controller()->set_extended_drag_available_for_testing(true);

    EXPECT_FALSE(window_->HasPointerFocus());
    EXPECT_EQ(State::kIdle, drag_controller()->state());
  }

  WaylandWindowDragController* drag_controller() const {
    return connection_->window_drag_controller();
  }

  wl::SerialTracker& serial_tracker() { return connection_->serial_tracker(); }

  MockWaylandPlatformWindowDelegate& delegate() { return delegate_; }
  WaylandWindow* window() { return window_.get(); }

 protected:
  using State = WaylandWindowDragController::State;

  void SendPointerEnter(WaylandWindow* window,
                        MockPlatformWindowDelegate* delegate) override {
    EXPECT_CALL(*delegate, DispatchEvent(_)).Times(1);
    WaylandDragDropTest::SendPointerEnter(window, delegate);
    Mock::VerifyAndClearExpectations(delegate);
    EXPECT_EQ(window,
              window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  }

  void SendPointerLeave(WaylandWindow* window,
                        MockPlatformWindowDelegate* delegate) override {
    EXPECT_CALL(*delegate, DispatchEvent(_)).Times(1);
    WaylandDragDropTest::SendPointerLeave(window, delegate);
    Mock::VerifyAndClearExpectations(delegate);
    EXPECT_EQ(nullptr,
              window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  }

  void SendPointerPress(WaylandWindow* window,
                        MockPlatformWindowDelegate* delegate,
                        int button) {
    EXPECT_CALL(*delegate, DispatchEvent(_)).Times(1);
    WaylandDragDropTest::SendPointerButton(window, delegate, button,
                                           /*pressed=*/true);
    Mock::VerifyAndClearExpectations(delegate);
    EXPECT_EQ(window,
              window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  }

  void SendPointerMotion(WaylandWindow* window,
                         MockPlatformWindowDelegate* delegate,
                         gfx::Point location,
                         bool ensure_dispatched = true) {
    if (ensure_dispatched) {
      EXPECT_CALL(*delegate, DispatchEvent(_)).WillOnce([](Event* event) {
        EXPECT_TRUE(event->IsMouseEvent());
        EXPECT_EQ(ET_MOUSE_DRAGGED, event->type());
      });
    }

    PostToServerAndWait([location](wl::TestWaylandServerThread* server) {
      wl_fixed_t x = wl_fixed_from_int(location.x());
      wl_fixed_t y = wl_fixed_from_int(location.y());
      ASSERT_TRUE(server->seat()->pointer());
      wl_resource* pointer_resource = server->seat()->pointer()->resource();
      wl_pointer_send_motion(pointer_resource, server->GetNextTime(), x, y);
      wl_pointer_send_frame(pointer_resource);
    });

    if (ensure_dispatched) {
      Mock::VerifyAndClearExpectations(delegate);
      EXPECT_EQ(window->GetWidget(),
                screen_->GetLocalProcessWidgetAtPoint(location, {}));
    }
  }

  // TODO(crbug.com/1116431): Support extended-drag in test compositor.

  void SendTouchDown(WaylandWindow* window,
                     MockPlatformWindowDelegate* delegate,
                     int id,
                     const gfx::Point& location) override {
    EXPECT_CALL(*delegate, DispatchEvent(_)).Times(1);
    WaylandDragDropTest::SendTouchDown(window, delegate, id, location);
    Mock::VerifyAndClearExpectations(delegate);
    EXPECT_EQ(window,
              window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  }
  void SendTouchMotion(WaylandWindow* window,
                       MockPlatformWindowDelegate* delegate,
                       int id,
                       const gfx::Point& location) override {
    EXPECT_CALL(*delegate, DispatchEvent(_)).WillOnce([](Event* event) {
      EXPECT_TRUE(event->IsTouchEvent());
      EXPECT_EQ(ET_TOUCH_MOVED, event->type());
    });
    WaylandDragDropTest::SendTouchMotion(window, delegate, id, location);
    Mock::VerifyAndClearExpectations(delegate);
    EXPECT_EQ(window->GetWidget(),
              screen_->GetLocalProcessWidgetAtPoint(location, {}));
  }
};

// Check the following flow works as expected:
// 1. With a single 1 window open,
// 2. Move pointer into it, press left button, move cursor a bit (drag),
// 3. Run move loop, drag it within the window bounds and drop.
TEST_P(WaylandWindowDragControllerTest, DragInsideWindowAndDrop) {
  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});

  // Set up an "interaction flow", start the drag session and run move loop:
  //  - Event dispatching and bounds changes are monitored
  //  - At each event, emulates a new event at server side and proceeds to
  // the next test step.
  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);

  enum { kStarted, kDragging, kDropping, kDone } test_step = kStarted;

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kStarted:
        EXPECT_EQ(ET_MOUSE_ENTERED, event->type());
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));
        // Drag it a bit more. We are in the middle of
        // WaylandWindowDragController::OnDragEnter. Run this via a task run.
        // Otherwise, the data offer will be reset and
        // WaylandWindowDragController will crash.
        ScheduleTestTask(base::BindOnce(&WaylandDragDropTest::SendDndMotion,
                                        base::Unretained(this),
                                        gfx::Point(20, 20)));
        test_step = kDragging;
        break;
      case kDropping: {
        EXPECT_EQ(ET_MOUSE_RELEASED, event->type());
        EXPECT_EQ(State::kDropped, drag_controller()->state());
        // Ensure PlatformScreen keeps consistent.
        gfx::Point expected_point{20, 20};
        expected_point += window_->GetBoundsInDIP().origin().OffsetFromOrigin();
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        test_step = kDone;
        break;
      }
      case kDone:
        EXPECT_EQ(ET_MOUSE_EXITED, event->type());
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        break;
      case kDragging:
      default:
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller()->state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(gfx::Point(20, 20), window_->GetBoundsInDIP().origin());
        EXPECT_TRUE(change.origin_changed);
        SendDndDrop();
        test_step = kDropping;
      });

  // RunMoveLoop() blocks until the dragging session ends.
  EXPECT_TRUE(move_loop_handler->RunMoveLoop({}));
  SendPointerEnter(window_.get(), &delegate_);

  EXPECT_EQ(State::kIdle, drag_controller()->state());
  EXPECT_EQ(window_.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Check the following flow works as expected:
// 1. With a single window open,
// 2. Touch down and move the touch point a bit (drag),
// 3. Run move loop, drag it within the window bounds and drop.
TEST_P(WaylandWindowDragControllerTest, DragInsideWindowAndDrop_TOUCH) {
  ASSERT_TRUE(GetWmMoveLoopHandler(*window_));
  ASSERT_TRUE(GetWaylandExtension(*window_));

  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  SendTouchDown(window_.get(), &delegate_, 0 /*point id*/, {0, 0} /*location*/);
  SendTouchMotion(window_.get(), &delegate_, 0 /*point id*/,
                  {10, 10} /*location*/);

  // Set up an "interaction flow", start the drag session and run move loop:
  //  - Event dispatching and bounds changes are monitored
  //  - At each event, emulates a new event at server side and proceeds to the
  //  next test step.
  GetWaylandExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);

  // While in |kAttached| state, motion events are expected to be dispatched
  // plain ET_TOUCH_MOVED events.
  EXPECT_EQ(State::kAttached, drag_controller()->state());
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_EQ(ET_TOUCH_MOVED, event->type());
    EXPECT_EQ(gfx::Point(10, 10), screen_->GetCursorScreenPoint());
  });
  SendDndMotion({10, 10});

  enum TestStep { kDragging, kDropping, kDone } test_step = kDragging;

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    ASSERT_EQ(kDropping, test_step);
    EXPECT_EQ(ET_TOUCH_RELEASED, event->type());
    EXPECT_EQ(State::kDropped, drag_controller()->state());
    // Ensure PlatformScreen keeps consistent.
    gfx::Point expected_point{20, 20};
    expected_point += window_->GetBoundsInDIP().origin().OffsetFromOrigin();
    EXPECT_EQ(expected_point, screen_->GetCursorScreenPoint());
    EXPECT_EQ(window_->GetWidget(),
              screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
    test_step = kDone;
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(gfx::Point(20, 20), window_->GetBoundsInDIP().origin());
        EXPECT_TRUE(change.origin_changed);

        test_step = kDropping;
        SendDndDrop();
      });

  // While in |kDetached| state, motion events are expected to be propagated
  // window bounds changed events.
  // Otherwise, we are not able to override the dispatcher and miss events.
  // This task must be scheduled so that move loop is able to be started before
  // this task is executed.
  ScheduleTestTask(base::BindOnce(&WaylandDragDropTest::SendDndMotion,
                                  base::Unretained(this),
                                  gfx::Point({20, 20})));

  // RunMoveLoop() blocks until the dragging session ends, so resume test
  // server's run loop until it returns.
  EXPECT_TRUE(GetWmMoveLoopHandler(*window_)->RunMoveLoop({}));

  EXPECT_EQ(test_step, TestStep::kDone);

  SendPointerEnter(window_.get(), &delegate_);

  EXPECT_EQ(State::kIdle, drag_controller()->state());
  EXPECT_EQ(window_.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Check the following flow works as expected:
// 1. Start dragging  window_2 with touch.
// 2. Emulate |window_2| being closed manually by the user (eg control+w).
// 3. No crash observed.
TEST_P(WaylandWindowDragControllerTest, DestroyWindowDuringDragAndDrop_TOUCH) {
  // Init and open |window_2|.
  PlatformWindowInitProperties properties{gfx::Rect{80, 80}};
  properties.type = PlatformWindowType::kWindow;
  EXPECT_CALL(delegate_, OnAcceleratedWidgetAvailable(_)).Times(1);
  auto window_2 =
      delegate_.CreateWaylandWindow(connection_.get(), std::move(properties));
  ASSERT_NE(gfx::kNullAcceleratedWidget, window_2->GetWidget());

  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  // Start a window dragging session and verify |window_2| is effectively being
  // dragged
  SendTouchDown(window_2.get(), &delegate_, 0 /*point id*/,
                {0, 0} /*location*/);

  auto* wayland_extension = GetWaylandExtension(*window_2);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);

  // Verify that the proper window is being dragged.
  EXPECT_EQ(window_2.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window_2.get(), drag_controller()->origin_window_for_testing());
  Mock::VerifyAndClearExpectations(&delegate_);

  // Destroy the dragged window, and expect no crashes.
  window_2.reset();
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());

  SendTouchUp(0 /*touch id*/);
}

// Check the following flow works as expected:
// 1. With two windows open,
// 2. Touch down and start drag a window,
// 3. Emulate the compositor sending an unexpected `pointer enter` event
//   to another window, when the drag is ongoing.
//
// NOTE: This bug isn't noticed on DUT, but seems to be frequent on ash/chrome
// linux desktop builds (with ozone/x11 underneath).
TEST_P(WaylandWindowDragControllerTest,
       DragAndDropWithExtraneousPointerEnterEvent_TOUCH) {
  // Init and open |target_window|.
  PlatformWindowInitProperties properties{gfx::Rect{80, 80}};
  properties.type = PlatformWindowType::kWindow;
  EXPECT_CALL(delegate_, OnAcceleratedWidgetAvailable(_)).Times(1);
  auto window_2 =
      delegate_.CreateWaylandWindow(connection_.get(), std::move(properties));
  ASSERT_NE(gfx::kNullAcceleratedWidget, window_2->GetWidget());

  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  ASSERT_TRUE(GetWmMoveLoopHandler(*window_2));
  ASSERT_TRUE(GetWaylandExtension(*window_2));

  // Start triggering a drag operation.
  SendTouchDown(window_2.get(), &delegate_, 0 /*point id*/,
                {0, 0} /*location*/);

  // This operation simulates bogus Wayland compositor that might send out
  // unexpected pointer enter events.
  SendPointerEnter(window_.get(), &delegate_);
  EXPECT_EQ(window_.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window_.get(), window_manager()->GetCurrentPointerFocusedWindow());
  EXPECT_EQ(window_2.get(), window_manager()->GetCurrentTouchFocusedWindow());

  // Set up an "interaction flow", start the drag session, run move loop
  // and verify the window effectively being dragged.
  GetWaylandExtension(*window_2)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);

  // Verify that the proper window is being dragged.
  EXPECT_EQ(window_2.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window_2.get(), drag_controller()->origin_window_for_testing());

  SendTouchUp(0 /*touch id*/);
}

// Check the following flow works as expected:
// 1. With only 1 window open;
// 2. Move pointer into it, press left button, move cursor a bit (drag);
// 3. Run move loop,
// 4. Drag pointer to outside the window and release the mouse button, and make
//    sure RELEASE and EXIT mouse events are delivered even when the drop
//    happens outside the bounds of any surface.
TEST_P(WaylandWindowDragControllerTest, DragExitWindowAndDrop) {
  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});

  // Sets up an "interaction flow", start the drag session and run move loop:
  //  - Event dispatching and bounds changes are monitored
  //  - At each event, emulates a new event on server side and proceeds to the
  //  next test step.
  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);

  enum { kStarted, kDragging, kExitedDropping, kDone } test_step = kStarted;

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kStarted:
        EXPECT_EQ(ET_MOUSE_ENTERED, event->type());
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));
        // Drag it a bit more. We are in the middle of
        // WaylandWindowDragController::OnDragEnter. Run this via a task run.
        // Otherwise, the data offer will be reset and
        // WaylandWindowDragController will crash.
        ScheduleTestTask(base::BindOnce(&WaylandDragDropTest::SendDndMotion,
                                        base::Unretained(this),
                                        gfx::Point(20, 20)));
        test_step = kDragging;
        break;
      case kExitedDropping: {
        EXPECT_EQ(ET_MOUSE_RELEASED, event->type());
        EXPECT_EQ(State::kDropped, drag_controller()->state());
        // Ensure PlatformScreen keeps consistent.
        gfx::Point expected_point{20, 20};
        expected_point += window_->GetBoundsInDIP().origin().OffsetFromOrigin();
        EXPECT_EQ(expected_point, screen_->GetCursorScreenPoint());
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        test_step = kDone;
      } break;
      case kDone:
        EXPECT_EQ(ET_MOUSE_EXITED, event->type());
        break;
      case kDragging:
      default:
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller()->state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(gfx::Point(20, 20), window_->GetBoundsInDIP().origin());
        EXPECT_TRUE(change.origin_changed);

        SendDndLeave();
        SendDndDrop();
        test_step = kExitedDropping;
      });

  // RunMoveLoop() blocks until the dragging sessions ends.
  EXPECT_TRUE(move_loop_handler->RunMoveLoop({}));

  SendPointerEnter(window_.get(), &delegate_);

  EXPECT_EQ(State::kIdle, drag_controller()->state());
  EXPECT_EQ(window_.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Check the following flow works as expected:
// 1. With 2 windows open,
// 2. Focus window 1, starts dragging,
// 3. Run move loop,
// 4. Drag the pointer out of window 1 and then into window 2,
// 5. Drag it a bit more (within window 2) and then calls EndMoveLoop(),
//    emulating a window snap), and then
// 6. With the window in "snapped" state, drag it further and then drop.
TEST_P(WaylandWindowDragControllerTest, DragToOtherWindowSnapDragDrop) {
  // Init and open |target_window|.
  PlatformWindowInitProperties properties{gfx::Rect{80, 80}};
  properties.type = PlatformWindowType::kWindow;
  EXPECT_CALL(delegate_, OnAcceleratedWidgetAvailable(_)).Times(1);
  auto window_2 =
      delegate_.CreateWaylandWindow(connection_.get(), std::move(properties));
  ASSERT_NE(gfx::kNullAcceleratedWidget, window_2->GetWidget());

  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  auto* source_window = window_.get();
  auto* target_window = window_2.get();
  EXPECT_TRUE(source_window);
  EXPECT_TRUE(target_window);

  SendPointerEnter(source_window, &delegate_);
  SendPointerPress(source_window, &delegate_, BTN_LEFT);
  SendPointerMotion(source_window, &delegate_, {10, 10});

  // Sets up an "interaction flow", start the drag session and run move loop:
  //  - Event dispatching and bounds changes are monitored
  //  - At each event, emulates a new event on server side and proceeds to the
  //  next test step.
  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);

  enum {
    kStarted,
    kDragging,
    kEnteredTarget,
    kSnapped,
    kDone
  } test_step = kStarted;

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kStarted:
        EXPECT_EQ(ET_MOUSE_ENTERED, event->type());
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(source_window->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));
        // Drag it a bit more. We are in the middle of
        // WaylandWindowDragController::OnDragEnter. Run this via a task run.
        // Otherwise, the data offer will be reset and
        // WaylandWindowDragController will crash.
        ScheduleTestTask(base::BindOnce(&WaylandDragDropTest::SendDndMotion,
                                        base::Unretained(this),
                                        gfx::Point(50, 50)));
        test_step = kDragging;
        break;
      case kEnteredTarget:
        EXPECT_EQ(ET_MOUSE_ENTERED, event->type());
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(target_window->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

        move_loop_handler->EndMoveLoop();
        test_step = kSnapped;
        break;
      default:
        move_loop_handler->EndMoveLoop();
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller()->state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(gfx::Point(50, 50), window_->GetBoundsInDIP().origin());
        EXPECT_TRUE(change.origin_changed);

        // Exit |source_window| and enter the |target_window|.
        SendDndLeave();
        test_step = kEnteredTarget;
        SendDndEnter(target_window, {});
      });

  // RunMoveLoop() blocks until the dragging sessions ends.
  // TODO(nickdiego): Should succeed for this test case.
  EXPECT_FALSE(move_loop_handler->RunMoveLoop({}));

  // Continue the dragging session after "snapping" the window. At this point,
  // the DND session is expected to be still alive and responding normally to
  // data object events.
  EXPECT_EQ(State::kAttached, drag_controller()->state());
  EXPECT_EQ(kSnapped, test_step);

  // Drag the pointer a bit more within |target_window| and then releases the
  // mouse button and ensures drag controller delivers the events properly and
  // exit gracefully.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(5);
  gfx::Point location(30, 30);
  for (size_t count = 1; count <= 5; ++count) {
    SendDndMotion(location);
    location.Offset(0, 3);
  }

  EXPECT_EQ(gfx::Point(30, 42), screen_->GetCursorScreenPoint());
  EXPECT_EQ(target_window->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({50, 50}, {}));

  // Emulates a pointer::leave event being sent before data_source::cancelled,
  // what happens with some compositors, e.g: Exosphere. Even in these cases,
  // WaylandWindowDragController must guarantee the mouse button release event
  // (aka: drop) is delivered to the upper layer listeners.
  //
  // TODO(https://crbug.com/1405471): Replace the block below by a call to
  // `SendPointerLeave(target_window, &delegate_)` when the bypass to pointer
  // enter|leave events is removed from
  // WaylandPointer::Enter|Leave().
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  WaylandDragDropTest::SendPointerLeave(target_window, &delegate_);
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_EQ(target_window,
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kSnapped:
        EXPECT_EQ(ET_MOUSE_RELEASED, event->type());
        EXPECT_EQ(State::kDropped, drag_controller()->state());
        EXPECT_EQ(target_window,
                  window_manager()->GetCurrentPointerOrTouchFocusedWindow());
        test_step = kDone;
        break;
      default:
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller()->state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  SendDndDrop();
  SendPointerEnter(target_window, &delegate_);
  EXPECT_EQ(target_window,
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(target_window->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Check the following flow works as expected:
// 1. With 2 windows open,
// 2. Focus window 1, starts dragging,
// 3. Run move loop,
// 4. Drag the pointer out of window 1 and then into window 2,
// 5. Drag it a bit more (within window 2) and then calls EndMoveLoop(),
//    emulating a window snap), and then
// 6. With the window in "snapped" state, drag it further and then drop.
TEST_P(WaylandWindowDragControllerTest, DragToOtherWindowSnapDragDrop_TOUCH) {
  // Init and open |target_window|.
  PlatformWindowInitProperties properties{gfx::Rect{80, 80}};
  properties.type = PlatformWindowType::kWindow;
  EXPECT_CALL(delegate_, OnAcceleratedWidgetAvailable(_)).Times(1);
  auto window_2 =
      delegate_.CreateWaylandWindow(connection_.get(), std::move(properties));
  ASSERT_NE(gfx::kNullAcceleratedWidget, window_2->GetWidget());

  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  auto* source_window = window_.get();
  auto* target_window = window_2.get();
  EXPECT_TRUE(source_window);
  EXPECT_TRUE(target_window);

  SendTouchDown(window_.get(), &delegate_, 0 /*point id*/, {0, 0} /*location*/);
  SendTouchMotion(window_.get(), &delegate_, 0 /*point id*/,
                  {10, 10} /*location*/);

  // Sets up an "interaction flow", start the drag session and run move loop:
  //  - Event dispatching and bounds changes are monitored
  //  - At each event, emulates a new event on server side and proceeds to the
  //  next test step.
  GetWaylandExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller()->state());
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_EQ(ET_TOUCH_MOVED, event->type());
    EXPECT_EQ(gfx::Point(10, 10), screen_->GetCursorScreenPoint());
  });
  SendDndMotion({10, 10});

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);

  enum {
    kStarted,
    kDragging,
    kEnteredTarget,
    kSnapped,
    kDone
  } test_step = kStarted;

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(gfx::Point(50, 50), window_->GetBoundsInDIP().origin());
        EXPECT_TRUE(change.origin_changed);

        // Exit |source_window| and enter the |target_window|.
        SendDndLeave();
        test_step = kEnteredTarget;
        SendDndEnter(target_window, {20, 20});
        move_loop_handler->EndMoveLoop();
      });

  // While in |kDetached| state, motion events are expected to be propagated
  // window bounds changed events.
  test_step = kDragging;
  ScheduleTestTask(base::BindOnce(&WaylandDragDropTest::SendDndMotion,
                                  base::Unretained(this), gfx::Point(50, 50)));

  // RunMoveLoop() blocks until the window moving ends.
  // TODO(nickdiego): Should succeed for this test case.
  EXPECT_FALSE(move_loop_handler->RunMoveLoop({}));

  // Checks |target_window| is now "focused" and the states keep consistent.
  EXPECT_EQ(kEnteredTarget, test_step);
  EXPECT_EQ(State::kAttached, drag_controller()->state());
  EXPECT_EQ(target_window->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));

  // Emulate |window_| snapping into |target_window|, and then continue the
  // dragging session after "snapping" process. At this point, the DND session
  // is expected to be still alive and responding normally to data object
  // events.
  window_.reset();
  test_step = kSnapped;
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  // Drag the pointer a bit more within |target_window| and then releases the
  // mouse button and ensures drag controller delivers the events properly and
  // exit gracefully.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(5);
  gfx::Point location(30, 30);
  for (size_t count = 1; count <= 5; ++count) {
    SendDndMotion(location);
    location.Offset(0, 3);
  }

  EXPECT_EQ(gfx::Point(30, 42), screen_->GetCursorScreenPoint());
  EXPECT_EQ(target_window->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({50, 50}, {}));

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_TRUE(event->IsTouchEvent());
    switch (test_step) {
      case kSnapped:
        EXPECT_EQ(ET_TOUCH_RELEASED, event->type());
        EXPECT_EQ(State::kDropped, drag_controller()->state());
        EXPECT_EQ(target_window,
                  window_manager()->GetCurrentPointerOrTouchFocusedWindow());
        test_step = kDone;
        break;
      default:
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller()->state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  SendDndDrop();
  SendTouchUp(0 /*touch id*/);
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
}

// Check the following flow works as expected:
// 1. With 2 windows open,
// 2. Focus window 1, starts dragging,
// 3. Run move loop,
// 4. Drag the pointer out of window 1 and then into window 2,
// 5. Simulate a spurious `wl_pointer.enter` event from the server
//    during the dnd operation. It should be ignored...
// 6. Drag it a bit more (within window 2) and then calls EndMoveLoop(),
//    emulating a window snap), and then drop.
TEST_P(WaylandWindowDragControllerTest,
       DragToOtherWindowIgnoringSpuriousPointerEnterEvent) {
  // Init and open |target_window|.
  PlatformWindowInitProperties properties{gfx::Rect{80, 80}};
  properties.type = PlatformWindowType::kWindow;
  EXPECT_CALL(delegate_, OnAcceleratedWidgetAvailable(_)).Times(1);
  auto window_2 =
      delegate_.CreateWaylandWindow(connection_.get(), std::move(properties));
  ASSERT_NE(gfx::kNullAcceleratedWidget, window_2->GetWidget());

  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  auto* source_window = window_.get();
  auto* target_window = window_2.get();
  EXPECT_TRUE(source_window);
  EXPECT_TRUE(target_window);

  SendPointerEnter(source_window, &delegate_);
  SendPointerPress(source_window, &delegate_, BTN_LEFT);
  SendPointerMotion(source_window, &delegate_, {10, 10});

  // Sets up an "interaction flow", start the drag session and run move loop:
  //  - Event dispatching and bounds changes are monitored
  //  - At each event, emulates a new event on server side and proceeds to the
  //  next test step.
  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);

  enum {
    kStarted,
    kDragging,
    kEnteredTarget,
    kSnapped,
    kDone
  } test_step = kStarted;

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kStarted:
        EXPECT_EQ(ET_MOUSE_ENTERED, event->type());
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(source_window->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));
        // Drag it a bit more. We are in the middle of
        // WaylandWindowDragController::OnDragEnter. Run this via a task run.
        // Otherwise, the data offer will be reset and
        // WaylandWindowDragController will crash.
        ScheduleTestTask(base::BindOnce(&WaylandDragDropTest::SendDndMotion,
                                        base::Unretained(this),
                                        gfx::Point(50, 50)));
        test_step = kDragging;
        break;
      case kEnteredTarget:
        EXPECT_EQ(ET_MOUSE_ENTERED, event->type());
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(target_window->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

        // Simulate a spurious `wl_pointer.enter` event to the |source_window|.
        // This should be ignored given that a window dnd operation is in place.
        //
        // NOTE: SendPointerEnter() isn't used here given that it sets
        // expectations that won't be met.
        WaylandDragDropTest::SendPointerEnter(source_window, &delegate_);
        EXPECT_EQ(target_window->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

        move_loop_handler->EndMoveLoop();
        test_step = kSnapped;
        break;
      default:
        move_loop_handler->EndMoveLoop();
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller()->state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(gfx::Point(50, 50), window_->GetBoundsInDIP().origin());
        EXPECT_TRUE(change.origin_changed);

        // Exit |source_window| and enter the |target_window|.
        SendDndLeave();
        test_step = kEnteredTarget;
        SendDndEnter(target_window, {});
      });

  // RunMoveLoop() blocks until the dragging sessions ends.
  // TODO(nickdiego): Should succeed for this test case.
  EXPECT_FALSE(move_loop_handler->RunMoveLoop({}));

  EXPECT_EQ(State::kAttached, drag_controller()->state());
  EXPECT_EQ(kSnapped, test_step);

  Mock::VerifyAndClearExpectations(&delegate_);

  SendDndDrop();
  SendPointerEnter(target_window, &delegate_);
  EXPECT_EQ(target_window,
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(target_window->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Verifies wl_data_device::leave events are properly handled and propagated
// while in window dragging "attached" mode.
TEST_P(WaylandWindowDragControllerTest, DragExitAttached) {
  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  auto* wayland_extension = GetWaylandExtension(*window_);
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  // Emulate a [motion => leave] event sequence and make sure the correct
  // ui::Events are dispatched in response.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  SendDndMotion({50, 50});

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_EQ(ET_MOUSE_DRAGGED, event->type());
    EXPECT_EQ(gfx::Point(50, -1).ToString(),
              event->AsMouseEvent()->location().ToString());
  });
  SendDndLeave();

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  SendDndDrop();

  SendPointerEnter(window_.get(), &delegate_);

  EXPECT_EQ(window_.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Verifies wl_data_device::leave events are properly handled and propagated
// while in window dragging "attached" mode.
TEST_P(WaylandWindowDragControllerTest, DragExitAttached_TOUCH) {
  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  SendTouchDown(window_.get(), &delegate_, 0 /*point id*/, {0, 0} /*location*/);
  SendTouchMotion(window_.get(), &delegate_, 0 /*point id*/,
                  {10, 10} /*location*/);
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  // Emulate a [motion => leave] event sequence and make sure the correct
  // ui::Events are dispatched in response.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  SendDndMotion({50, 50});

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_EQ(ET_TOUCH_MOVED, event->type());
    EXPECT_EQ(gfx::Point(50, -1000).ToString(),
              event->AsTouchEvent()->location().ToString());
  });
  SendDndLeave();

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  SendDndDrop();
}

using BoundsChange = PlatformWindowDelegate::BoundsChange;

TEST_P(WaylandWindowDragControllerTest, RestoreDuringWindowDragSession) {
  const gfx::Rect original_bounds = window_->GetBoundsInDIP();
  wl::ScopedWlArray states({XDG_TOPLEVEL_STATE_ACTIVATED});

  // Maximize and check restored bounds is correctly set.
  constexpr gfx::Rect kMaximizedBounds{1024, 768};
  EXPECT_CALL(delegate_, OnBoundsChanged(testing::Eq(BoundsChange(false))));
  window_->Maximize();
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED);
  SendConfigureEvent(window_->root_surface()->get_surface_id(),
                     kMaximizedBounds.size(), states);

  EXPECT_EQ(kMaximizedBounds, window_->GetBoundsInDIP());

  auto restored_bounds = window_->GetRestoredBoundsInDIP();
  EXPECT_EQ(original_bounds, restored_bounds);

  // Start a window drag session.
  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(WaylandWindowDragController::State::kAttached,
            connection_->window_drag_controller()->state());

  // Call restore and ensure it's no-op.
  window_->Restore();
  EXPECT_EQ(PlatformWindowState::kMaximized, window_->GetPlatformWindowState());
}

// Check the following flow works as expected:
//
// 1. With a single 1 window open,
// 2. Move pointer into it, press left button, move cursor a bit (drag),
// 3. Run move loop, drag it from 200,200 location to 100,100
// 4. Send a few wl_pointer::motion events targeting 20,20 location and ensure
//    they are ignored (i.e: window bounds keep unchanged) until drop happens.
//
// Verifies window drag controller is resistant to issues such as
// https://crbug.com/1148021.
TEST_P(WaylandWindowDragControllerTest, IgnorePointerEventsUntilDrop) {
  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({200, 200}, {}));

  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {200, 200});

  // Set up an "interaction flow", start the drag session and run move loop:
  //  - Event dispatching and bounds changes are monitored
  //  - At each event, emulates a new event at server side and proceeds to the
  //  next test step.
  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);

  enum { kStarted, kDragging, kDropping, kDone } test_step = kStarted;

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kStarted:
        EXPECT_EQ(ET_MOUSE_ENTERED, event->type());
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({200, 200}, {}));
        // Drag it a bit more. We are in the middle of
        // WaylandWindowDragController::OnDragEnter. Run this via a task run.
        // Otherwise, the data offer will be reset and
        // WaylandWindowDragController will crash.
        ScheduleTestTask(base::BindOnce(&WaylandDragDropTest::SendDndMotion,
                                        base::Unretained(this),
                                        gfx::Point(100, 100)));
        test_step = kDragging;
        break;
      case kDropping: {
        EXPECT_EQ(ET_MOUSE_RELEASED, event->type());
        EXPECT_EQ(State::kDropped, drag_controller()->state());

        // Ensure |window_|'s bounds did not change in response to 20,20
        // wl_pointer::motion events sent at |kDragging| test step.
        EXPECT_EQ(gfx::kNullAcceleratedWidget,
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({100, 100}, {}));

        // Rather, only PlatformScreen's cursor position is updated accordingly.
        gfx::Point expected_point{20, 20};
        expected_point += window_->GetBoundsInDIP().origin().OffsetFromOrigin();
        EXPECT_EQ(expected_point, screen_->GetCursorScreenPoint());
        test_step = kDone;
      } break;
      case kDone:
        EXPECT_EQ(ET_MOUSE_EXITED, event->type());
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({100, 100}, {}));
        break;
      case kDragging:
      default:
        FAIL() << " event=" << event->ToString()
               << " state=" << drag_controller()->state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_TRUE(change.origin_changed);
        EXPECT_EQ(gfx::Point(100, 100), window_->GetBoundsInDIP().origin());
        EXPECT_TRUE(change.origin_changed);

        // WaylandWindowDragController might be in the middle of something, when
        // calling this. However, given issuing commands to a server result in
        // running a RunLoop, the actual commands will result in events that the
        // client receives. That will result in a new flow while we are blocked
        // here, which means WaylandWindowDragController will not complete what
        // it must complete before getting the below commands processed by the
        // server and received by the client. Thus, prepare a task to avoid that
        // and let the WaylandWindowDragController to do what it needs to do.
        base::OnceClosure send_pointer_motion_30_30 = base::BindOnce(
            &WaylandWindowDragControllerTest_IgnorePointerEventsUntilDrop_Test::
                SendPointerMotion,
            base::Unretained(this), nullptr, nullptr, gfx::Point(30, 30),
            false);
        base::OnceClosure send_pointer_motion_20_20 = base::BindOnce(
            &WaylandWindowDragControllerTest_IgnorePointerEventsUntilDrop_Test::
                SendPointerMotion,
            base::Unretained(this), nullptr, nullptr, gfx::Point(20, 20),
            false);
        base::OnceClosure send_drop = base::BindOnce(
            &WaylandDragDropTest::SendDndDrop, base::Unretained(this));

        auto test = [](base::OnceClosure send_pointer_motion_30_30,
                       base::OnceClosure send_pointer_motion_20_20,
                       base::OnceClosure send_dnd_drop) {
          // Send a few wl_pointer::motion events skipping sync and dispatch
          // checks, which will be done at |kDropping| test step handling.
          std::move(send_pointer_motion_30_30).Run();
          std::move(send_pointer_motion_20_20).Run();
          std::move(send_dnd_drop).Run();
        };
        test_step = kDropping;
        auto closure_cb = base::BindLambdaForTesting(std::move(test));
        ScheduleTestTask(base::BindOnce(
            std::move(closure_cb), std::move(send_pointer_motion_30_30),
            std::move(send_pointer_motion_20_20), std::move(send_drop)));
      });

  // RunMoveLoop() blocks until the dragging session ends.
  EXPECT_TRUE(move_loop_handler->RunMoveLoop({}));

  SendPointerEnter(window_.get(), &delegate_);

  EXPECT_EQ(State::kIdle, drag_controller()->state());
  EXPECT_EQ(window_.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({100, 100}, {}));
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Regression test for https://crbug.com/1169446.
TEST_P(WaylandWindowDragControllerTest, MotionEventsSkippedWhileReattaching) {
  auto* dragged_window = window_.get();
  EXPECT_TRUE(dragged_window);

  SendPointerEnter(dragged_window, &delegate_);
  SendPointerPress(dragged_window, &delegate_, BTN_LEFT);
  SendPointerMotion(dragged_window, &delegate_, {10, 10});

  // Ensure the controller's state is updated accordingly and a MOUSE_ENTERED
  // event is dispatched in response to wl_data_device.enter.
  EXPECT_CALL(delegate(), DispatchEvent(_)).Times(1);
  // Start the drag session.
  GetWaylandExtension(*dragged_window)
      ->StartWindowDraggingSessionIfNeeded(DragEventSource::kMouse,
                                           /*allow_system_drag=*/false);
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*dragged_window);
  ASSERT_TRUE(move_loop_handler);

  auto test = [](WaylandWindowDragControllerTest* self,
                 WmMoveLoopHandler* move_loop_handler) {
    // While in |kDetached| state, motion events are expected to be propagated
    // by window drag controller as bounds changes.
    EXPECT_EQ(State::kDetached, self->drag_controller()->state());
    EXPECT_CALL(self->delegate(), OnBoundsChanged(_))
        .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
          EXPECT_EQ(gfx::Point(30, 30),
                    self->window()->GetBoundsInDIP().origin());
          EXPECT_TRUE(change.origin_changed);
        });
    self->SendDndMotion({30, 30});

    move_loop_handler->EndMoveLoop();

    // Otherwise, after the move loop is requested to quit, but before it really
    // ends (ie. kAttaching state), motion events are **not** expected to be
    // propagated.
    EXPECT_EQ(State::kAttaching, self->drag_controller()->state());
    EXPECT_CALL(self->delegate(), OnBoundsChanged(_)).Times(0);
    self->SendDndMotion({31, 31});
  };
  ScheduleTestTask(base::BindOnce(test, base::Unretained(this),
                                  base::Unretained(move_loop_handler)));

  // Spins move loop for |window_1|.
  // TODO(nickdiego): Should succeed for this test case.
  EXPECT_FALSE(move_loop_handler->RunMoveLoop({}));

  // When the transition to |kAttached| state is finally done (ie. nested loop
  // quits), motion events are then expected to be propagated by window drag
  // controller as usual.
  EXPECT_EQ(State::kAttached, drag_controller()->state());
  EXPECT_CALL(delegate(), DispatchEvent(_)).Times(1);
  SendDndMotion({30, 30});

  EXPECT_CALL(delegate(), DispatchEvent(_)).Times(1);
  SendDndDrop();

  SendPointerEnter(window_.get(), &delegate_);

  EXPECT_EQ(State::kIdle, drag_controller()->state());
}

// Test that cursor position is using DIP coordinates and is updated correctly
// on DragMotion event.
TEST_P(WaylandWindowDragControllerTest, CursorPositionIsUpdatedOnMotion) {
  constexpr gfx::Rect kOutputBounds(0, 0, 1920, 1080);
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    // Configure the first output with scale 1.
    wl::TestOutput* output1 = server->output();
    output1->SetPhysicalAndLogicalBounds(kOutputBounds);
    output1->Flush();

    // Creating a second output with scale 2.
    auto* output2 =
        server->CreateAndInitializeOutput(wl::TestOutputMetrics(kOutputBounds));
    output2->SetScale(2);
    output2->SetDeviceScaleFactor(2);
  });

  WaitForAllDisplaysReady();

  const std::vector<display::Display>& displays = screen_->GetAllDisplays();
  EXPECT_EQ(displays.size(), 2u);

  // Start a window drag session.
  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  gfx::Point p0{10, 10};
  SendPointerMotion(window_.get(), &delegate_, p0);
  EXPECT_EQ(p0, screen_->GetCursorScreenPoint());

  auto* wayland_extension = GetWaylandExtension(*window_);
  EXPECT_CALL(delegate(), DispatchEvent(_)).Times(::testing::AtLeast(2));
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  wl::SyncDisplay(connection_->display_wrapper(), *connection_->display());
  // Starting a DnD session results in a server sending a Enter event, which
  // enters the window at 0x0.
  EXPECT_EQ(gfx::Point(0, 0), screen_->GetCursorScreenPoint());
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  // Now move the pointer to 10x10 location and start the test.
  SendDndMotion(p0);
  EXPECT_EQ(p0, screen_->GetCursorScreenPoint());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);
  auto test = [](WaylandWindowDragControllerTest* self, WaylandScreen* screen,
                 const WaylandOutputManager::OutputList* outputs,
                 wl::TestWaylandServerThread* server, WaylandWindow* window,
                 WmMoveLoopHandler* move_loop_handler,
                 bool in_pixel_coordinates) {
    ASSERT_TRUE(outputs);
    for (const auto& output : *outputs) {
      SCOPED_TRACE(
          base::StringPrintf("Output Scale=%f", output.second->scale_factor()));
      gfx::Point p0{10, 10};
      // Compute the expected point first as drag operation will move the
      // window.
      gfx::Point expected_point =
          in_pixel_coordinates
              ? gfx::ScaleToRoundedPoint(
                    p0, 1.0f / window->applied_state().window_scale)
              : p0;
      expected_point += window->GetBoundsInDIP().origin().OffsetFromOrigin();
      EXPECT_EQ(expected_point, screen->GetCursorScreenPoint());

      // Resetting cursor to the initial position.
      self->SendDndMotion(p0);

      // Send the window to |output|.
      const uint32_t surface_id = window->root_surface()->get_surface_id();
      const uint32_t output_id = wl_proxy_get_id(
          reinterpret_cast<wl_proxy*>(output.second->get_output()));
      self->PostToServerAndWait([surface_id, output_id](
                                    wl::TestWaylandServerThread* server) {
        wl::MockSurface* surface =
            server->GetObject<wl::MockSurface>(surface_id);
        ASSERT_TRUE(surface);
        wl::TestOutput* output = server->GetObject<wl::TestOutput>(output_id);
        ASSERT_TRUE(output);
        wl_surface_send_enter(surface->resource(), output->resource());
      });
      EXPECT_EQ(output.second->scale_factor(),
                window->applied_state().window_scale);

      gfx::Point p1{20, 20};
      expected_point =
          (in_pixel_coordinates
               ? gfx::ScaleToRoundedPoint(
                     p1, 1.0f / window->applied_state().window_scale)
               : p1);
      expected_point += window->GetBoundsInDIP().origin().OffsetFromOrigin();

      self->SendDndMotion(p1);

      EXPECT_EQ(expected_point, screen->GetCursorScreenPoint());
      self->PostToServerAndWait([surface_id, output_id](
                                    wl::TestWaylandServerThread* server) {
        wl::MockSurface* surface =
            server->GetObject<wl::MockSurface>(surface_id);
        ASSERT_TRUE(surface);
        wl::TestOutput* output = server->GetObject<wl::TestOutput>(output_id);
        ASSERT_TRUE(output);

        wl_surface_send_leave(surface->resource(), output->resource());
      });
    }

    move_loop_handler->EndMoveLoop();
  };

  const WaylandOutputManager::OutputList* outputs =
      &connection_->wayland_output_manager()->GetAllOutputs();
  ScheduleTestTask(base::BindOnce(
      test, base::Unretained(this), base::Unretained(screen_.get()),
      base::Unretained(outputs), base::Unretained(&server_),
      base::Unretained(window_.get()), base::Unretained(move_loop_handler),
      connection_->surface_submission_in_pixel_coordinates()));
  move_loop_handler->RunMoveLoop({});
}

// Ensure no memory issues happen when the dragged window is destroyed just
// after quitting the move loop. Regression test for crbug.com/1267791 and
// should be caught in both regular and ASAN builds, where more details about
// the actual memory issue is provided.
TEST_P(WaylandWindowDragControllerTest,
       HandleDraggedWindowDestructionAfterMoveLoop) {
  // 1. Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});

  // 2. Start the window drag session.
  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);
  ScheduleTestTask(
      base::BindLambdaForTesting([&]() { move_loop_handler->EndMoveLoop(); }));

  // 3. Run the move loop.
  EXPECT_FALSE(move_loop_handler->RunMoveLoop({}));

  // 4. Destroy the dragged window just after quitting move loop.
  const auto* dangling_window_ptr = window_.get();
  window_.reset();
  EXPECT_NE(dangling_window_ptr, drag_controller()->pointer_grab_owner_);
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  // 5. Ensure no events are dispatched for drop. Which indirectly means that
  // drop handling code at window drag controller does not call into the above
  // destroyed dragged window.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  SendDndDrop();

  // 6. Verifies that related state is correctly reset after drop.
  EXPECT_EQ(State::kIdle, drag_controller()->state());
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Ensure no memory issues happen when the dragged and/or events grabber windows
// get destroyed while the move loop is running.
TEST_P(WaylandWindowDragControllerTest,
       HandleWindowsDestructionDuringMoveLoop) {
  // 1. Send some initial pointer events to |window_|.
  ASSERT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});

  // 2. Start the window drag session.
  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  // 3. Spawns a new toplevel |window_2| out of the origin |window_|. Similarly
  // to when a tab is detached in a Chrome's tab drag session.
  PlatformWindowInitProperties properties{gfx::Rect{80, 80}};
  properties.type = PlatformWindowType::kWindow;
  MockWaylandPlatformWindowDelegate delegate_2;
  EXPECT_CALL(delegate_2, OnAcceleratedWidgetAvailable(_)).Times(1);
  auto window_2 =
      delegate_2.CreateWaylandWindow(connection_.get(), std::move(properties));
  ASSERT_NE(gfx::kNullAcceleratedWidget, window_2->GetWidget());

  // Spin the nested move loop and schedule a sequence of test steps to be
  // pefomed while it is running.
  ScheduleTestTask(base::BindLambdaForTesting([&]() {
    auto* cursor_tracker = connection_->wayland_cursor_position();
    ASSERT_TRUE(cursor_tracker);

    // Send a motion event and verify it is correctly propagated.
    EXPECT_CALL(delegate_2, OnBoundsChanged(_)).Times(1);
    SendDndMotion({11, 10});
    EXPECT_EQ(gfx::Point(11, 10), cursor_tracker->GetCursorSurfacePoint());

    // Destroy the window being currently dragged.
    window_2.reset();

    // Verifies that motion events are no longer propagated, as the dragged
    // window was just destroyed.
    EXPECT_CALL(delegate_2, OnBoundsChanged(_)).Times(0);
    SendDndMotion({12, 10});
    EXPECT_EQ(gfx::Point(12, 10), cursor_tracker->GetCursorSurfacePoint());

    // Destroy the current drag controler's pointer events grabber.
    window_.reset();

    // And verifies neither motion events are propagated nor internal "last
    // cursor position" gets updated.
    EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
    SendDndMotion({13, 10});
    EXPECT_EQ(gfx::Point(12, 10), cursor_tracker->GetCursorSurfacePoint());

    drag_controller()->StopDragging();
  }));

  ASSERT_TRUE(GetWmMoveLoopHandler(*window_2));
  EXPECT_FALSE(GetWmMoveLoopHandler(*window_2)->RunMoveLoop({}));

  EXPECT_EQ(State::kAttached, drag_controller()->state());

  // 5. Ensure no events are dispatched for drop. Which indirectly means that
  // drop handling code at window drag controller does not call into the above
  // destroyed dragged window.
  EXPECT_CALL(delegate_2, DispatchEvent(_)).Times(0);
  SendDndDrop();

  // 6. Verifies that related state is correctly reset after drop.
  EXPECT_EQ(State::kIdle, drag_controller()->state());
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Ensures correct behavior when ext-drag protocol is not available, such as:
//
// 1. Returns 'success' even when wl_data_source.cancelled is sent by the
//    Wayland Compositor.
//
//  Regression test for https://crbug.com/1366504.
TEST_P(WaylandWindowDragControllerTest, ExtendedDragUnavailable) {
  ASSERT_TRUE(GetWmMoveLoopHandler(*window_));
  ASSERT_TRUE(GetWaylandExtension(*window_));
  drag_controller()->set_extended_drag_available_for_testing(false);

  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});

  GetWaylandExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);

  enum { kStarted, kDropping, kDone } test_step = kStarted;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kStarted:
        EXPECT_EQ(ET_MOUSE_ENTERED, event->type());
        EXPECT_EQ(State::kDetached, drag_controller()->state());

        // We are in the middle of
        // WaylandWindowDragController::OnDragEnter. Run this via a task run.
        // Otherwise, the data offer will be reset and
        // WaylandWindowDragController will crash.
        ScheduleTestTask(base::BindOnce(&WaylandDragDropTest::SendDndCancelled,
                                        base::Unretained(this)));
        test_step = kDropping;
        break;
      case kDropping: {
        EXPECT_EQ(ET_MOUSE_RELEASED, event->type());
        EXPECT_EQ(State::kDropped, drag_controller()->state());

        test_step = kDone;
        break;
      }
      case kDone:
        EXPECT_EQ(ET_MOUSE_EXITED, event->type());
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        break;
    }
  });

  // RunMoveLoop() blocks until the dragging session ends, so resume test
  // server's run loop until it returns.
  bool succeeded = move_loop_handler->RunMoveLoop({});
  EXPECT_TRUE(succeeded);

  SendPointerEnter(window_.get(), &delegate_);

  EXPECT_EQ(State::kIdle, drag_controller()->state());
  EXPECT_EQ(window_.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
}

TEST_P(WaylandWindowDragControllerTest, GetSerial) {
  auto* origin = static_cast<WaylandToplevelWindow*>(window_.get());
  auto& window_manager = *connection_->window_manager();

  window_manager.SetPointerFocusedWindow(nullptr);
  window_manager.SetTouchFocusedWindow(nullptr);
  serial_tracker().ClearForTesting();
  {  // No serial, no window focused.
    auto serial = drag_controller()->GetSerial(DragEventSource::kMouse, origin);
    EXPECT_FALSE(serial.has_value());
  }

  // Check cases where only pointer focus info is set.
  {  // Serial available, but no window focused.
    serial_tracker().UpdateSerial(wl::SerialType::kMousePress, 1u);
    auto serial = drag_controller()->GetSerial(DragEventSource::kMouse, origin);
    EXPECT_FALSE(serial.has_value());
  }

  {  // Both serial and focused window available.
    window_manager.SetPointerFocusedWindow(window_.get());
    auto serial = drag_controller()->GetSerial(DragEventSource::kMouse, origin);
    ASSERT_TRUE(serial.has_value());
    EXPECT_EQ(wl::SerialType::kMousePress, serial->type);
    EXPECT_EQ(1u, serial->value);
  }

  // Reset and check cases where only touch focus info is set.
  window_manager.SetPointerFocusedWindow(nullptr);
  window_manager.SetTouchFocusedWindow(nullptr);
  serial_tracker().ClearForTesting();
  {  // Serial available, but no window focused.
    serial_tracker().UpdateSerial(wl::SerialType::kTouchPress, 2u);
    auto serial = drag_controller()->GetSerial(DragEventSource::kTouch, origin);
    EXPECT_FALSE(serial.has_value());
  }
  {  // Both serial and focused window available.
    window_manager.SetTouchFocusedWindow(window_.get());
    auto serial = drag_controller()->GetSerial(DragEventSource::kTouch, origin);
    ASSERT_TRUE(serial.has_value());
    EXPECT_EQ(wl::SerialType::kTouchPress, serial->type);
    EXPECT_EQ(2u, serial->value);
  }
  // Reset focused window and serial information.
  window_manager.SetPointerFocusedWindow(nullptr);
  window_manager.SetTouchFocusedWindow(nullptr);
  serial_tracker().ClearForTesting();
}

TEST_P(WaylandWindowDragControllerTest, NoopUnlessPointerOrTouchPressed) {
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());

  // Press left mouse button within |window_|.
  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);

  // Drag mustn't start for touch source while there is no active touch points.
  GetWaylandExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);
  ASSERT_EQ(State::kIdle, drag_controller()->state());
  ASSERT_FALSE(drag_controller()->drag_source().has_value());
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    ASSERT_FALSE(server->data_device_manager()->data_source());
  });
  SendPointerButton(window_.get(), &delegate_, BTN_LEFT, /*pressed=*/false);

  ASSERT_FALSE(serial_tracker().GetSerial(wl::SerialType::kTouchPress));

  // Now it should start successfully.
  GetWaylandExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);
  ASSERT_EQ(State::kIdle, drag_controller()->state());
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandWindowDragControllerTest,
                         Values(wl::ServerConfig{}));

INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestWithAuraShell,
    WaylandWindowDragControllerTest,
    Values(wl::ServerConfig{.enable_aura_shell =
                                wl::EnableAuraShellProtocol::kEnabled},
           wl::ServerConfig{
               .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled,
               .use_aura_output_manager = true}));

}  // namespace ui
