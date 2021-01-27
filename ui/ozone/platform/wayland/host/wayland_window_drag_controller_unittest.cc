// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input-event-codes.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <xdg-shell-server-protocol.h>

#include <cstdint>

#include "base/bind.h"
#include "base/notreached.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/scoped_wl_array.h"
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_drag_drop_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"

using testing::_;
using testing::Mock;

namespace ui {

class WaylandWindowDragControllerTest : public WaylandDragDropTest {
 public:
  WaylandWindowDragControllerTest() = default;
  ~WaylandWindowDragControllerTest() override = default;

  void SetUp() override {
    WaylandDragDropTest::SetUp();

    screen_ = std::make_unique<WaylandScreen>(connection_.get());

    wl_seat_send_capabilities(server_.seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
    Sync();
    pointer_ = server_.seat()->pointer();
    ASSERT_TRUE(pointer_);

    EXPECT_FALSE(window_->has_pointer_focus());
    EXPECT_EQ(State::kIdle, drag_controller()->state());
  }

  WaylandWindowDragController* drag_controller() const {
    return connection_->window_drag_controller();
  }

  WaylandWindowManager* window_manager() const {
    return connection_->wayland_window_manager();
  }

  MockPlatformWindowDelegate& delegate() { return delegate_; }

 protected:
  using State = WaylandWindowDragController::State;

  void SendPointerEnter(WaylandWindow* window,
                        MockPlatformWindowDelegate* delegate) {
    auto* surface = server_.GetObject<wl::MockSurface>(
        window->root_surface()->GetSurfaceId());
    wl_pointer_send_enter(pointer_->resource(), NextSerial(),
                          surface->resource(), 0, 0);
    EXPECT_CALL(*delegate, DispatchEvent(_)).Times(1);
    Sync();

    EXPECT_EQ(window, window_manager()->GetCurrentFocusedWindow());
  }

  void SendPointerLeave(WaylandWindow* window,
                        MockPlatformWindowDelegate* delegate) {
    auto* surface = server_.GetObject<wl::MockSurface>(
        window->root_surface()->GetSurfaceId());
    wl_pointer_send_leave(pointer_->resource(), NextSerial(),
                          surface->resource());
    EXPECT_CALL(*delegate, DispatchEvent(_)).Times(1);
    Sync();

    EXPECT_EQ(nullptr, window_manager()->GetCurrentFocusedWindow());
  }

  void SendPointerPress(WaylandWindow* window,
                        MockPlatformWindowDelegate* delegate,
                        int button) {
    wl_pointer_send_button(pointer_->resource(), NextSerial(), NextTime(),
                           button, WL_POINTER_BUTTON_STATE_PRESSED);
    EXPECT_CALL(*delegate, DispatchEvent(_)).Times(1);
    Sync();

    EXPECT_EQ(window, window_manager()->GetCurrentFocusedWindow());
  }

  void SendPointerMotion(WaylandWindow* window,
                         MockPlatformWindowDelegate* delegate,
                         gfx::Point location,
                         bool sync_and_ensure_dispatched = true) {
    wl_fixed_t x = wl_fixed_from_int(location.x());
    wl_fixed_t y = wl_fixed_from_int(location.y());
    wl_pointer_send_motion(pointer_->resource(), NextTime(), x, y);

    if (!sync_and_ensure_dispatched)
      return;

    EXPECT_CALL(*delegate, DispatchEvent(_)).WillOnce([](Event* event) {
      EXPECT_TRUE(event->IsMouseEvent());
      EXPECT_EQ(ET_MOUSE_DRAGGED, event->type());
    });
    Sync();

    EXPECT_EQ(window->GetWidget(),
              screen_->GetLocalProcessWidgetAtPoint(location, {}));
  }

  // For the context of window drag, "drop" is detected through
  // wl_data_source::cancelled in the regular case. Unless extended-drag
  // protocol is available.
  //
  // TODO(crbug.com/1116431): Support extended-drag in test compositor.
  void SendDndDrop() { SendDndCancelled(); }

  // client objects
  std::unique_ptr<WaylandScreen> screen_;

  // server objects
  wl::MockPointer* pointer_;
};

// Check the following flow works as expected:
// 1. With a single 1 window open,
// 2. Move pointer into it, press left button, move cursor a bit (drag),
// 3. Run move loop, drag it within the window bounds and drop.
TEST_P(WaylandWindowDragControllerTest, DragInsideWindowAndDrop) {
  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});

  // Set up an "interaction flow", start the drag session and run move loop:
  //  - Event dispatching and bounds changes are monitored
  //  - At each event, emulates a new event at server side and proceeds to the
  //  next test step.
  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded();
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  DCHECK(move_loop_handler);

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
        // Drag it a bit more.
        SendDndMotion({20, 20});
        test_step = kDragging;
        break;
      case kDropping:
        EXPECT_EQ(ET_MOUSE_RELEASED, event->type());
        EXPECT_EQ(State::kDropped, drag_controller()->state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(gfx::Point(20, 20), screen_->GetCursorScreenPoint());
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        test_step = kDone;
        break;
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
        return;
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const gfx::Rect& bounds) {
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(gfx::Point(20, 20), bounds.origin());

        SendDndDrop();
        test_step = kDropping;
      });

  // RunMoveLoop() blocks until the dragging session ends, so resume test
  // server's run loop until it returns.
  server_.Resume();
  move_loop_handler->RunMoveLoop({});
  server_.Pause();

  SendPointerEnter(window_.get(), &delegate_);
  Sync();

  EXPECT_EQ(State::kIdle, drag_controller()->state());
  EXPECT_EQ(window_.get(), window_manager()->GetCurrentFocusedWindow());
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
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
  EXPECT_FALSE(window_manager()->GetCurrentFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});
  Sync();

  // Sets up an "interaction flow", start the drag session and run move loop:
  //  - Event dispatching and bounds changes are monitored
  //  - At each event, emulates a new event on server side and proceeds to the
  //  next test step.
  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded();
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  DCHECK(move_loop_handler);

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
        // Drag window a bit more.
        SendDndMotion({20, 20});
        test_step = kDragging;
        break;
      case kExitedDropping:
        EXPECT_EQ(ET_MOUSE_RELEASED, event->type());
        EXPECT_EQ(State::kDropped, drag_controller()->state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(gfx::Point(20, 20), screen_->GetCursorScreenPoint());
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        test_step = kDone;
        break;
      case kDone:
        EXPECT_EQ(ET_MOUSE_EXITED, event->type());
        break;
      case kDragging:
      default:
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller()->state()
               << " step=" << static_cast<int>(test_step);
        return;
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const gfx::Rect& bounds) {
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(gfx::Point(20, 20), bounds.origin());

        SendDndLeave();
        SendDndDrop();
        test_step = kExitedDropping;
      });

  // RunMoveLoop() blocks until the dragging sessions ends, so resume test
  // server's run loop until it returns.
  server_.Resume();
  move_loop_handler->RunMoveLoop({});
  server_.Pause();

  SendPointerEnter(window_.get(), &delegate_);
  Sync();

  EXPECT_EQ(State::kIdle, drag_controller()->state());
  EXPECT_EQ(window_.get(), window_manager()->GetCurrentFocusedWindow());
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
  auto window_2 = WaylandWindow::Create(&delegate_, connection_.get(),
                                        std::move(properties));
  ASSERT_NE(gfx::kNullAcceleratedWidget, window_2->GetWidget());
  Sync();

  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentFocusedWindow());
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
  wayland_extension->StartWindowDraggingSessionIfNeeded();
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  DCHECK(move_loop_handler);

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
        // Drag window a bit more.
        SendDndMotion({50, 50});
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
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller()->state()
               << " step=" << static_cast<int>(test_step);
        return;
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const gfx::Rect& bounds) {
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(gfx::Point(50, 50), bounds.origin());

        // Exit |source_window| and enter the |target_window|.
        SendDndLeave();
        SendDndEnter(target_window, {});
        test_step = kEnteredTarget;
      });

  // RunMoveLoop() blocks until the dragging sessions ends, so resume test
  // server's run loop until it returns.
  server_.Resume();
  move_loop_handler->RunMoveLoop({});
  server_.Pause();

  // Continue the dragging session after "snapping" the window. At this point,
  // the DND session is expected to be still alive and responding normally to
  // data object events.
  EXPECT_EQ(State::kAttached, drag_controller()->state());
  EXPECT_EQ(kSnapped, test_step);

  // Drag the pointer a bit more within |target_window| and then releases the
  // mouse button and ensures drag controller delivers the events properly and
  // exit gracefully.
  SendDndMotion({30, 30});
  SendDndMotion({30, 33});
  SendDndMotion({30, 36});
  SendDndMotion({30, 39});
  SendDndMotion({30, 42});
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(5);
  Sync();

  EXPECT_EQ(gfx::Point(30, 42), screen_->GetCursorScreenPoint());
  EXPECT_EQ(target_window->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({50, 50}, {}));

  // Emulates a pointer::leave event being sent before data_source::cancelled,
  // what happens with some compositors, e.g: Exosphere. Even in these cases,
  // WaylandWindowDragController must guarantee the mouse button release event
  // (aka: drop) is delivered to the upper layer listeners.
  SendPointerLeave(target_window, &delegate_);

  SendDndDrop();
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kSnapped:
        EXPECT_EQ(ET_MOUSE_RELEASED, event->type());
        EXPECT_EQ(State::kDropped, drag_controller()->state());
        EXPECT_EQ(target_window, window_manager()->GetCurrentFocusedWindow());
        test_step = kDone;
        break;
      default:
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller()->state()
               << " step=" << static_cast<int>(test_step);
        return;
    }
  });
  Sync();

  SendPointerEnter(target_window, &delegate_);
  EXPECT_EQ(target_window, window_manager()->GetCurrentFocusedWindow());
  EXPECT_EQ(target_window->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Verifies wl_data_device::leave events are properly handled and propagated
// while in window dragging "attached" mode.
TEST_P(WaylandWindowDragControllerTest, DragExitAttached) {
  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});
  Sync();
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded();
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  Sync();
  Sync();
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  // Emulate a [motion => leave] event sequence and make sure the correct
  // ui::Events are dispatched in response.
  SendDndMotion({50, 50});
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  Sync();

  SendDndLeave();
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_EQ(ET_MOUSE_DRAGGED, event->type());
    EXPECT_EQ(gfx::Point(50, -1).ToString(),
              event->AsMouseEvent()->location().ToString());
  });
  Sync();

  SendDndDrop();
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  Sync();

  SendPointerEnter(window_.get(), &delegate_);
  Sync();

  EXPECT_EQ(window_.get(), window_manager()->GetCurrentFocusedWindow());
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

TEST_P(WaylandWindowDragControllerTest, RestoreDuringWindowDragSession) {
  const gfx::Rect original_bounds = window_->GetBounds();
  wl::ScopedWlArray states({XDG_TOPLEVEL_STATE_ACTIVATED});

  // Maximize and check restored bounds is correctly set.
  const gfx::Rect maximized_bounds = gfx::Rect(0, 0, 1024, 768);
  EXPECT_CALL(delegate_, OnBoundsChanged(testing::Eq(maximized_bounds)));
  window_->Maximize();
  states.AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED);
  SendConfigureEvent(surface_->xdg_surface(), maximized_bounds.width(),
                     maximized_bounds.height(), 1, states.get());
  Sync();
  auto restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(original_bounds, restored_bounds);

  // Start a window drag session.
  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});
  Sync();
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded();
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
  EXPECT_FALSE(window_manager()->GetCurrentFocusedWindow());
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
  wayland_extension->StartWindowDraggingSessionIfNeeded();
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  DCHECK(move_loop_handler);

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
        // Drag it a bit more.
        SendDndMotion({100, 100});
        test_step = kDragging;
        break;
      case kDropping:
        EXPECT_EQ(ET_MOUSE_RELEASED, event->type());
        EXPECT_EQ(State::kDropped, drag_controller()->state());

        // Ensure |window_|'s bounds did not change in response to 20,20
        // wl_pointer::motion events sent at |kDragging| test step.
        EXPECT_EQ(gfx::kNullAcceleratedWidget,
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({100, 100}, {}));

        // Rather, only PlatformScreen's cursor position is updated accordingly.
        EXPECT_EQ(gfx::Point(20, 20), screen_->GetCursorScreenPoint());
        test_step = kDone;
        break;
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
        return;
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const gfx::Rect& bounds) {
        EXPECT_EQ(State::kDetached, drag_controller()->state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(gfx::Point(100, 100), bounds.origin());

        // Send a few wl_pointer::motion events skipping sync and dispatch
        // checks, which will be done at |kDropping| test step handling.
        SendPointerMotion(window_.get(), &delegate_, {30, 30},
                          /*sync_and_ensure_dispatched =*/false);
        SendPointerMotion(window_.get(), &delegate_, {20, 20},
                          /*sync_and_ensure_dispatched =*/false);

        SendDndDrop();
        test_step = kDropping;
      });

  // RunMoveLoop() blocks until the dragging session ends, so resume test
  // server's run loop until it returns.
  server_.Resume();
  move_loop_handler->RunMoveLoop({});
  server_.Pause();

  SendPointerEnter(window_.get(), &delegate_);
  Sync();

  EXPECT_EQ(State::kIdle, drag_controller()->state());
  EXPECT_EQ(window_.get(), window_manager()->GetCurrentFocusedWindow());
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

  auto* wayland_extension = GetWaylandExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded();
  EXPECT_EQ(State::kAttached, drag_controller()->state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);

  auto test = [](WaylandWindowDragControllerTest* self,
                 WmMoveLoopHandler* move_loop_handler) {
    // While in |kDetached| state, motion events are expected to be propagated
    // by window drag controller.
    EXPECT_EQ(State::kDetached, self->drag_controller()->state());
    self->SendDndMotion({30, 30});
    EXPECT_CALL(self->delegate(), DispatchEvent(_)).Times(1);
    self->Sync();

    move_loop_handler->EndMoveLoop();
    self->Sync();

    // Otherwise, after the move loop is requested to quit, but before it really
    // ends (ie. kAttaching state), motion events are **not** expected to be
    // propagated.
    EXPECT_EQ(State::kAttaching, self->drag_controller()->state());
    self->SendDndMotion({30, 30});
    EXPECT_CALL(self->delegate(), DispatchEvent(_)).Times(0);
    self->Sync();
  };
  ScheduleTestTask(base::BindOnce(test, base::Unretained(this),
                                  base::Unretained(move_loop_handler)));

  // Spins move loop for |window_1|.
  move_loop_handler->RunMoveLoop({});

  // When the transition to |kAttached| state is finally done (ie. nested loop
  // quits), motion events are then expected to be propagated by window drag
  // controller as usual.
  EXPECT_EQ(State::kAttached, drag_controller()->state());
  SendDndMotion({30, 30});
  EXPECT_CALL(delegate(), DispatchEvent(_)).Times(1);
  Sync();

  SendDndDrop();
  EXPECT_CALL(delegate(), DispatchEvent(_)).Times(1);
  Sync();

  SendPointerEnter(window_.get(), &delegate_);

  EXPECT_EQ(State::kIdle, drag_controller()->state());
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandWindowDragControllerTest,
                         ::testing::Values(kXdgShellStable));

INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandWindowDragControllerTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
