// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"

#include <linux/input-event-codes.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <xdg-shell-server-protocol.h>

#include <cstdint>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
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
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zaura_toplevel.h"
#include "ui/ozone/platform/wayland/test/wayland_drag_drop_test.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/platform/wayland/test/wayland_window_drag_controller_test_api.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"

using testing::_;
using testing::Mock;
using testing::Values;

namespace ui {

using mojom::DragEventSource;
using TestApi = WaylandWindowDragControllerTestApi;

class WaylandWindowDragControllerTest : public WaylandDragDropTest {
 public:
  WaylandWindowDragControllerTest() = default;
  ~WaylandWindowDragControllerTest() override = default;

  void SetUp() override {
    WaylandDragDropTest::SetUp();
    TestApi(drag_controller()).set_extended_drag_available(true);

    EXPECT_FALSE(window_->HasPointerFocus());
    EXPECT_EQ(State::kIdle, drag_controller_state());
  }

  WaylandWindowDragController* drag_controller() {
    return connection_->window_drag_controller();
  }

  WaylandWindowDragController::State drag_controller_state() {
    return TestApi(drag_controller()).state();
  }

  WaylandPointer::Delegate* pointer_delegate() {
    return TestApi(drag_controller()).pointer_delegate();
  }

  WaylandTouch::Delegate* touch_delegate() {
    return TestApi(drag_controller()).touch_delegate();
  }

  wl::SerialTracker& serial_tracker() { return connection_->serial_tracker(); }

  MockWaylandPlatformWindowDelegate& delegate() { return delegate_; }
  WaylandWindow* window() { return window_.get(); }

  void SendPointerMotion(WaylandWindow* window,
                         MockPlatformWindowDelegate* delegate,
                         gfx::Point location,
                         bool ensure_dispatched = true) {
    if (ensure_dispatched) {
      EXPECT_CALL(*delegate, DispatchEvent(_)).WillOnce([](Event* event) {
        EXPECT_TRUE(event->IsMouseEvent());
        EXPECT_EQ(EventType::kMouseDragged, event->type());
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

  void SendDndMotionForWindowDrag(const gfx::Point& location) {
    WaylandDragDropTest::SendDndMotion(location);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Emulate the server side logic during move loop. The server
    // server controls the bounds only when the window is detached.
    if (drag_controller_state() !=
        WaylandWindowDragController::State::kDetached) {
      return;
    }
    // The window must exist. (should not be swallowed nor destroyed)
    ASSERT_TRUE(window_);
    auto& offset = TestApi(drag_controller()).drag_offset();
    gfx::Point new_origin = (location - offset);
    auto* dragged_window = TestApi(drag_controller()).dragged_window();
    ASSERT_TRUE(dragged_window);
    const uint32_t surface_id =
        dragged_window->root_surface()->get_surface_id();
    PostToServerAndWait(
        [new_origin, surface_id](wl::TestWaylandServerThread* server) {
          auto* surface = server->GetObject<wl::MockSurface>(surface_id);
          ASSERT_TRUE(surface);
          ASSERT_TRUE(surface->xdg_surface());
          ASSERT_TRUE(surface->xdg_surface()->xdg_toplevel());

          auto* aura_toplevel =
              surface->xdg_surface()->xdg_toplevel()->zaura_toplevel();
          ASSERT_TRUE(aura_toplevel);
          zaura_toplevel_send_origin_change(aura_toplevel->resource(),
                                            new_origin.x(), new_origin.y());
        });
#endif
  }

  void SendDndDropAndFinished() {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* data_source = server->data_device_manager()->data_source();
      ASSERT_TRUE(data_source);
      data_source->OnDndDropPerformed();
      data_source->OnFinished();
    });
  }

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

  // TODO(crbug.com/40711933): Support extended-drag in test compositor.

  void SendTouchDown(WaylandWindow* window,
                     MockPlatformWindowDelegate* delegate,
                     int id,
                     const gfx::Point& location) override {
    EXPECT_CALL(*delegate, DispatchEvent(_)).WillOnce([](Event* event) {
      EXPECT_EQ(EventType::kTouchPressed, event->type());
    });
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
      EXPECT_EQ(EventType::kTouchMoved, event->type());
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
  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);

  enum { kStarted, kDragging, kDropping, kDone } test_step = kStarted;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kStarted:
        EXPECT_EQ(EventType::kMouseEntered, event->type());
        EXPECT_EQ(State::kDetached, drag_controller_state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));
        // Drag it a bit more. We are in the middle of OnDragEnter. Run this via
        // a task run. Otherwise, the data offer will be reset and
        // WaylandWindowDragController will crash.
        ScheduleTestTask(base::BindOnce(
            &WaylandWindowDragControllerTest::SendDndMotionForWindowDrag,
            base::Unretained(this), gfx::Point(20, 20)));
        test_step = kDragging;
        break;
      case kDropping: {
        if (event->type() == EventType::kMouseEntered) {
          EXPECT_EQ(window_.get(),
                    window_manager()->GetCurrentPointerFocusedWindow());
          return;
        }
        EXPECT_EQ(EventType::kMouseReleased, event->type());
        EXPECT_EQ(State::kDropped, drag_controller_state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        test_step = kDone;
        break;
      }
      case kDragging:
      default:
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller_state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(State::kDetached, drag_controller_state());
        EXPECT_EQ(gfx::Point(20, 20), window_->GetBoundsInDIP().origin());
        EXPECT_TRUE(change.origin_changed);
        test_step = kDropping;
        SendDndDropAndFinished();
      });

  // RunMoveLoop() blocks until the dragging session ends.
  EXPECT_TRUE(move_loop_handler->RunMoveLoop({}));

  // Verify expectations.
  Mock::VerifyAndClearExpectations(&delegate_);
  ASSERT_EQ(test_step, kDone);
  EXPECT_EQ(State::kIdle, drag_controller_state());
  EXPECT_EQ(window_.get(), window_manager()->GetCurrentPointerFocusedWindow());
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Check the following flow works as expected:
// 1. With a single window open,
// 2. Touch down and move the touch point a bit (drag),
// 3. Run move loop, drag it within the window bounds and drop.
TEST_P(WaylandWindowDragControllerTest, DragInsideWindowAndDrop_TOUCH) {
  ASSERT_TRUE(GetWmMoveLoopHandler(*window_));
  ASSERT_TRUE(GetWaylandToplevelExtension(*window_));

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
  GetWaylandToplevelExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);

  // While in |kAttached| state, motion events are expected to be dispatched
  // plain EventType::kTouchMoved events.
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_EQ(EventType::kTouchMoved, event->type());
    EXPECT_EQ(gfx::Point(10, 10), event->AsLocatedEvent()->root_location());
    EXPECT_EQ(State::kAttached, drag_controller_state());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // On Lacros, touch event will not update the cursor position.
    EXPECT_EQ(gfx::Point(0, 0), screen_->GetCursorScreenPoint());
#else
    EXPECT_EQ(gfx::Point(10, 10), screen_->GetCursorScreenPoint());
#endif
  });
  SendDndMotionForWindowDrag({10, 10});

  enum TestStep { kDragging, kDropping, kDone } test_step = kDragging;

  EXPECT_CALL(delegate_, DispatchEvent(_))
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      // Lacros dispatches TOUCH_MOVED event so that aura can update the touch
      // position.
      .WillOnce([&](Event* event) {
        EXPECT_EQ(EventType::kTouchMoved, event->type());
        EXPECT_EQ(gfx::Point(20, 20), event->AsLocatedEvent()->root_location());
      })
#endif
      .WillOnce([&](Event* event) {
        EXPECT_EQ(EventType::kTouchReleased, event->type());
        ASSERT_EQ(kDropping, test_step);
        EXPECT_EQ(State::kDropped, drag_controller_state());
    // Ensure PlatformScreen keeps consistent.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        gfx::Point expected_point{0, 0};
    // On Lacros, touch event will not update the cursor position.
#else
        gfx::Point expected_point{20, 20};
        expected_point += window_->GetBoundsInDIP().origin().OffsetFromOrigin();
#endif
        EXPECT_EQ(expected_point, screen_->GetCursorScreenPoint());
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        test_step = kDone;
      });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(State::kDetached, drag_controller_state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(gfx::Point(20, 20), window_->GetBoundsInDIP().origin());
        EXPECT_TRUE(change.origin_changed);

        test_step = kDropping;
        ScheduleTestTask(base::BindOnce(
            &WaylandWindowDragControllerTest::SendDndDropAndFinished,
            base::Unretained(this)));
      });

  // While in |kDetached| state, motion events are expected to be propagated
  // window bounds changed events.
  // Otherwise, we are not able to override the dispatcher and miss events.
  // This task must be scheduled so that move loop is able to be started before
  // this task is executed.
  ScheduleTestTask(base::BindOnce(
      &WaylandWindowDragControllerTest::SendDndMotionForWindowDrag,
      base::Unretained(this), gfx::Point({20, 20})));

  // RunMoveLoop() blocks until the dragging session ends, so resume test
  // server's run loop until it returns.
  EXPECT_TRUE(GetWmMoveLoopHandler(*window_)->RunMoveLoop({}));

  EXPECT_EQ(test_step, TestStep::kDone);

  SendPointerEnter(window_.get(), &delegate_);

  EXPECT_EQ(State::kIdle, drag_controller_state());
  EXPECT_EQ(window_.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Similar to DragInsideWindowAndDrop_TOUCH but with two fingers.
TEST_P(WaylandWindowDragControllerTest, DragInsideWindowAndDropTwoFingerTouch) {
  ASSERT_TRUE(GetWmMoveLoopHandler(*window_));
  ASSERT_TRUE(GetWaylandToplevelExtension(*window_));

  // Ensure there is no window currently focused
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  // Touch and then move with two fingers.
  SendTouchDown(window_.get(), &delegate_, 0 /*point id*/, {0, 0} /*location*/);
  SendTouchDown(window_.get(), &delegate_, 1 /*point id*/, {5, 0} /*location*/);
  SendTouchMotion(window_.get(), &delegate_, 0 /*point id*/,
                  {10, 10} /*location*/);
  SendTouchMotion(window_.get(), &delegate_, 1 /*point id*/,
                  {15, 10} /*location*/);

  // Set up an "interaction flow", start the drag session and run move loop:
  //  - Event dispatching and bounds changes are monitored
  //  - At each event, emulates a new event at server side and proceeds to the
  //  next test step.
  GetWaylandToplevelExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);

  // While in |kAttached| state, motion events are expected to be dispatched
  // plain EventType::kTouchMoved events.
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .WillOnce([&](Event* event) {
        EXPECT_EQ(EventType::kTouchMoved, event->type());
        EXPECT_EQ(0, event->AsTouchEvent()->pointer_details().id);
        EXPECT_EQ(gfx::Point(10, 10), event->AsLocatedEvent()->root_location());
        EXPECT_EQ(State::kAttached, drag_controller_state());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        // On Lacros, touch event will not update the cursor position.
        EXPECT_EQ(gfx::Point(0, 0), screen_->GetCursorScreenPoint());
#else
        EXPECT_EQ(gfx::Point(10, 10), screen_->GetCursorScreenPoint());
#endif
      })
      .WillOnce([&](Event* event) {
        EXPECT_EQ(EventType::kTouchMoved, event->type());
        EXPECT_EQ(1, event->AsTouchEvent()->pointer_details().id);
        EXPECT_EQ(gfx::Point(10, 10), event->AsLocatedEvent()->root_location());
        EXPECT_EQ(State::kAttached, drag_controller_state());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        // On Lacros, touch event will not update the cursor position.
        EXPECT_EQ(gfx::Point(0, 0), screen_->GetCursorScreenPoint());
#else
        EXPECT_EQ(gfx::Point(10, 10), screen_->GetCursorScreenPoint());
#endif
      });
  SendDndMotionForWindowDrag({10, 10});

  enum TestStep {
    kDragging,
    kDropping,
    kFirstFingerReleased,
    kDone
  } test_step = kDragging;

  EXPECT_CALL(delegate_, DispatchEvent(_))
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      // Lacros dispatches TOUCH_MOVED event so that aura can update the touch
      // position.
      .WillOnce([&](Event* event) {
        ASSERT_EQ(kDragging, test_step);
        EXPECT_EQ(EventType::kTouchMoved, event->type());
        EXPECT_EQ(0, event->AsTouchEvent()->pointer_details().id);
        EXPECT_EQ(State::kDetached, drag_controller_state());
        EXPECT_EQ(gfx::Point(20, 20), event->AsLocatedEvent()->root_location());
      })
      .WillOnce([&](Event* event) {
        ASSERT_EQ(kDragging, test_step);
        EXPECT_EQ(EventType::kTouchMoved, event->type());
        EXPECT_EQ(1, event->AsTouchEvent()->pointer_details().id);

        EXPECT_EQ(State::kDetached, drag_controller_state());
        EXPECT_EQ(gfx::Point(20, 20), event->AsLocatedEvent()->root_location());
      })
#endif
      // delegate_ should receive two touch release events in a sequence.
      .WillOnce([&](Event* event) {
        ASSERT_EQ(kDropping, test_step);
        EXPECT_EQ(EventType::kTouchReleased, event->type());
        EXPECT_EQ(0, event->AsTouchEvent()->pointer_details().id);
        EXPECT_EQ(State::kDropped, drag_controller_state());

    // Ensure PlatformScreen keeps consistent.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        gfx::Point expected_point{0, 0};
    // On Lacros, touch event will not update the cursor position.
#else
        gfx::Point expected_point{20, 20};
        expected_point += window_->GetBoundsInDIP().origin().OffsetFromOrigin();
#endif
        EXPECT_EQ(expected_point, screen_->GetCursorScreenPoint());

        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        test_step = kFirstFingerReleased;
      })
      .WillOnce([&](Event* event) {
        ASSERT_EQ(kFirstFingerReleased, test_step);
        EXPECT_EQ(EventType::kTouchReleased, event->type());
        EXPECT_EQ(1, event->AsTouchEvent()->pointer_details().id);
        EXPECT_EQ(State::kDropped, drag_controller_state());

    // Ensure PlatformScreen keeps consistent.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        gfx::Point expected_point{0, 0};
    // On Lacros, touch event will not update the cursor position.
#else
        gfx::Point expected_point{20, 20};
        expected_point += window_->GetBoundsInDIP().origin().OffsetFromOrigin();
#endif
        EXPECT_EQ(expected_point, screen_->GetCursorScreenPoint());

        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        test_step = kDone;
      });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        ASSERT_EQ(kDragging, test_step);
        EXPECT_EQ(State::kDetached, drag_controller_state());
        EXPECT_EQ(gfx::Point(20, 20), window_->GetBoundsInDIP().origin());
        EXPECT_TRUE(change.origin_changed);

        test_step = kDropping;
        ScheduleTestTask(base::BindOnce(
            &WaylandWindowDragControllerTest::SendDndDropAndFinished,
            base::Unretained(this)));
      });

  // While in |kDetached| state, motion events are expected to be propagated
  // window bounds changed events.
  // Otherwise, we are not able to override the dispatcher and miss events.
  // This task must be scheduled so that move loop is able to be started before
  // this task is executed.
  ScheduleTestTask(base::BindOnce(
      &WaylandWindowDragControllerTest::SendDndMotionForWindowDrag,
      base::Unretained(this), gfx::Point({20, 20})));

  // RunMoveLoop() blocks until the dragging session ends, so resume test
  // server's run loop until it returns.
  EXPECT_TRUE(GetWmMoveLoopHandler(*window_)->RunMoveLoop({}));

  EXPECT_EQ(test_step, TestStep::kDone);

  SendPointerEnter(window_.get(), &delegate_);

  EXPECT_EQ(State::kIdle, drag_controller_state());
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

  auto* wayland_extension = GetWaylandToplevelExtension(*window_2);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);

  // Verify that the proper window is being dragged.
  EXPECT_EQ(window_2.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window_2.get(), TestApi(drag_controller()).origin_window());
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
  ASSERT_TRUE(GetWaylandToplevelExtension(*window_2));

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
  GetWaylandToplevelExtension(*window_2)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);

  // Verify that the proper window is being dragged.
  EXPECT_EQ(window_2.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window_2.get(), TestApi(drag_controller()).origin_window());

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
  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);

  enum { kStarted, kDragging, kExitedDropping, kDone } test_step = kStarted;

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kStarted:
        EXPECT_EQ(EventType::kMouseEntered, event->type());
        EXPECT_EQ(State::kDetached, drag_controller_state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));
        // Drag it a bit more. We are in the middle of OnDragEnter. Run this via
        // a task run. Otherwise, the data offer will be reset and
        // WaylandWindowDragController will crash.
        ScheduleTestTask(base::BindOnce(
            &WaylandWindowDragControllerTest::SendDndMotionForWindowDrag,
            base::Unretained(this), gfx::Point(20, 20)));
        test_step = kDragging;
        break;
      case kExitedDropping: {
        if (event->type() == EventType::kMouseEntered) {
          EXPECT_EQ(window_.get(),
                    window_manager()->GetCurrentPointerFocusedWindow());
          return;
        }
        EXPECT_EQ(EventType::kMouseReleased, event->type());
        EXPECT_EQ(State::kDropped, drag_controller_state());
        // Ensure PlatformScreen keeps consistent.
        gfx::Point expected_point{20, 20};
        expected_point += window_->GetBoundsInDIP().origin().OffsetFromOrigin();
        EXPECT_EQ(expected_point, screen_->GetCursorScreenPoint());
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        test_step = kDone;
      } break;
      case kDragging:
      default:
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller_state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(State::kDetached, drag_controller_state());
        EXPECT_EQ(gfx::Point(20, 20), window_->GetBoundsInDIP().origin());
        EXPECT_TRUE(change.origin_changed);

        test_step = kExitedDropping;
        SendDndLeave();
        SendDndDropAndFinished();
      });

  // RunMoveLoop() blocks until the dragging sessions ends.
  EXPECT_TRUE(move_loop_handler->RunMoveLoop({}));

  // Verify expectations.
  Mock::VerifyAndClearExpectations(&delegate_);
  ASSERT_EQ(test_step, kDone);
  EXPECT_EQ(State::kIdle, drag_controller_state());
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
// TODO(crbug.com/40886646): Test needs to be updated for 112.0.5570.0 to remove
// special handling logic in wayland_pointer.cc.
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
  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());

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
        EXPECT_EQ(EventType::kMouseEntered, event->type());
        EXPECT_EQ(State::kDetached, drag_controller_state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(source_window->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));
        // Drag it a bit more. We are in the middle of
        // WaylandWindowDragController::OnDragEnter. Run this via a task run.
        // Otherwise, the data offer will be reset and
        // WaylandWindowDragController will crash.
        ScheduleTestTask(base::BindOnce(
            &WaylandWindowDragControllerTest::SendDndMotionForWindowDrag,
            base::Unretained(this), gfx::Point(50, 50)));
        test_step = kDragging;
        break;
      case kEnteredTarget:
        EXPECT_EQ(EventType::kMouseEntered, event->type());
        EXPECT_EQ(State::kDetached, drag_controller_state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(target_window->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

        move_loop_handler->EndMoveLoop();
        test_step = kSnapped;
        break;
      default:
        move_loop_handler->EndMoveLoop();
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller_state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(State::kDetached, drag_controller_state());
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
  EXPECT_EQ(State::kAttached, drag_controller_state());
  EXPECT_EQ(kSnapped, test_step);

  // Drag the pointer a bit more within |target_window| and then releases the
  // mouse button and ensures drag controller delivers the events properly and
  // exit gracefully.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(5);
  gfx::Point location(30, 30);
  for (size_t count = 1; count <= 5; ++count) {
    SendDndMotionForWindowDrag(location);
    location.Offset(0, 3);
  }

  EXPECT_EQ(gfx::Point(30, 42), screen_->GetCursorScreenPoint());
  EXPECT_EQ(target_window->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({50, 50}, {}));

  // Emulates a pointer::leave event being sent before data_source::cancelled,
  // what happens with some compositors, e.g: Mutter, KWin. Even in these cases,
  // WaylandWindowDragController must guarantee the mouse button release event
  // (aka: drop) is delivered to the upper layer listeners.
  //
  // TODO(crbug.com/329479345): Revisit once drop/cancellation handling is
  // refactored in drag controller.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  WaylandDragDropTest::SendPointerLeave(target_window, &delegate_);
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_EQ(target_window,
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kSnapped:
        EXPECT_EQ(EventType::kMouseReleased, event->type());
        EXPECT_EQ(State::kDropped, drag_controller_state());
        EXPECT_EQ(target_window,
                  window_manager()->GetCurrentPointerOrTouchFocusedWindow());
        test_step = kDone;
        break;
      default:
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller_state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  SendDndDropAndFinished();
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
  GetWaylandToplevelExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());
  EXPECT_CALL(delegate_, DispatchEvent(_))
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      // Lacros dispatches TOUCH_MOVED event so that aura can update the touch
      // position.
      .WillOnce([&](Event* event) {
        EXPECT_EQ(EventType::kTouchMoved, event->type());
        EXPECT_EQ(gfx::Point(10, 10), event->AsLocatedEvent()->root_location());
      })
#endif
      .WillOnce([&](Event* event) {
        EXPECT_EQ(EventType::kTouchMoved, event->type());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        // On Lacros, touch event will not update the cursor position.
        EXPECT_EQ(gfx::Point(0, 0), screen_->GetCursorScreenPoint());
#else
        EXPECT_EQ(gfx::Point(10, 10), screen_->GetCursorScreenPoint());
#endif
      });
  SendDndMotionForWindowDrag({10, 10});

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
        EXPECT_EQ(State::kDetached, drag_controller_state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_EQ(gfx::Point(50, 50), window_->GetBoundsInDIP().origin());
        EXPECT_TRUE(change.origin_changed);

        // Exit |source_window| and enter the |target_window|.
        SendDndLeave();
        test_step = kEnteredTarget;
        SendDndEnter(target_window, {20, 20});
        // Ensure the target window has focus.
        EXPECT_EQ(target_window,
                  window_manager()->GetCurrentPointerOrTouchFocusedWindow());
        EXPECT_EQ(target_window, static_cast<WaylandTouch::Delegate*>(
                                     connection_->event_source())
                                     ->GetTouchTarget(0 /*point id*/));
        move_loop_handler->EndMoveLoop();
      });

  // While in |kDetached| state, motion events are expected to be propagated
  // window bounds changed events.
  test_step = kDragging;
  ScheduleTestTask(base::BindOnce(
      &WaylandWindowDragControllerTest::SendDndMotionForWindowDrag,
      base::Unretained(this), gfx::Point(50, 50)));

  // RunMoveLoop() blocks until the window moving ends.
  // TODO(nickdiego): Should succeed for this test case.
  EXPECT_FALSE(move_loop_handler->RunMoveLoop({}));

  // Checks |target_window| is now "focused" and the states keep consistent.
  EXPECT_EQ(kEnteredTarget, test_step);
  EXPECT_EQ(State::kAttached, drag_controller_state());
  EXPECT_EQ(target_window->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));

  // Emulate |window_| snapping into |target_window|, and then continue the
  // dragging session after "snapping" process. At this point, the DND session
  // is expected to be still alive and responding normally to data object
  // events.
  window_.reset();
  test_step = kSnapped;
  EXPECT_EQ(State::kAttached, drag_controller_state());

  // Drag the pointer a bit more within |target_window| and then releases the
  // mouse button and ensures drag controller delivers the events properly and
  // exit gracefully.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(5);
  gfx::Point location(30, 30);
  for (size_t count = 1; count <= 5; ++count) {
    SendDndMotionForWindowDrag(location);
    location.Offset(0, 3);
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros, touch event will not update the cursor position.
  EXPECT_EQ(gfx::Point(0, 0), screen_->GetCursorScreenPoint());
#else
  EXPECT_EQ(gfx::Point(30, 42), screen_->GetCursorScreenPoint());
#endif
  EXPECT_EQ(target_window->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({50, 50}, {}));

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_TRUE(event->IsTouchEvent());
    switch (test_step) {
      case kSnapped:
        EXPECT_EQ(EventType::kTouchReleased, event->type());
        EXPECT_EQ(State::kDropped, drag_controller_state());
        EXPECT_EQ(target_window,
                  window_manager()->GetCurrentPointerOrTouchFocusedWindow());
        test_step = kDone;
        break;
      default:
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller_state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  SendDndDropAndFinished();
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
// TODO(crbug.com/40886646): Test needs to be updated for 112.0.5570.0 to remove
// special handling logic in wayland_pointer.cc.
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
  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());

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
        EXPECT_EQ(EventType::kMouseEntered, event->type());
        EXPECT_EQ(State::kDetached, drag_controller_state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(source_window->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));
        // Drag it a bit more. We are in the middle of
        // WaylandWindowDragController::OnDragEnter. Run this via a task run.
        // Otherwise, the data offer will be reset and
        // WaylandWindowDragController will crash.
        ScheduleTestTask(base::BindOnce(
            &WaylandWindowDragControllerTest::SendDndMotionForWindowDrag,
            base::Unretained(this), gfx::Point(50, 50)));
        test_step = kDragging;
        break;
      case kEnteredTarget:
        EXPECT_EQ(EventType::kMouseEntered, event->type());
        EXPECT_EQ(State::kDetached, drag_controller_state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(target_window->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

        // Simulate a spurious `wl_pointer.enter` event to the |source_window|.
        // This should be ignored given that a window dnd operation is in place.
        //
        // TODO(crbug.com/329479345): Revisit once drop/cancellation handling is
        // refactored in drag controller.
        WaylandDragDropTest::SendPointerEnter(source_window, &delegate_);
        EXPECT_EQ(target_window->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

        move_loop_handler->EndMoveLoop();
        test_step = kSnapped;
        break;
      default:
        move_loop_handler->EndMoveLoop();
        FAIL() << " event=" << event->GetName()
               << " state=" << drag_controller_state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(State::kDetached, drag_controller_state());
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

  EXPECT_EQ(State::kAttached, drag_controller_state());
  EXPECT_EQ(kSnapped, test_step);

  Mock::VerifyAndClearExpectations(&delegate_);

  SendDndDropAndFinished();
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

  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  WaylandTestBase::SyncDisplay();
  EXPECT_EQ(State::kAttached, drag_controller_state());

  // Emulate a [motion => leave] event sequence and make sure the correct
  // ui::Events are dispatched in response.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  SendDndMotionForWindowDrag({50, 50});

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_EQ(EventType::kMouseDragged, event->type());
    EXPECT_EQ(gfx::Point(50, -1).ToString(),
              event->AsMouseEvent()->location().ToString());
  });
  SendDndLeave();

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  SendDndDropAndFinished();

  SendPointerEnter(window_.get(), &delegate_);

  EXPECT_EQ(window_.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  if (window_->IsScreenCoordinatesEnabled()) {
    EXPECT_EQ(window_->GetWidget(),
              screen_->GetLocalProcessWidgetAtPoint({70, 70}, {}));
  } else {
    EXPECT_EQ(window_->GetWidget(),
              screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
  }
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

  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());

  // Emulate a [motion => leave] event sequence and make sure the correct
  // ui::Events are dispatched in response.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  SendDndMotionForWindowDrag({50, 50});

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce([&](Event* event) {
    EXPECT_EQ(EventType::kTouchMoved, event->type());
    EXPECT_EQ(gfx::Point(50, -10).ToString(),
              event->AsTouchEvent()->location().ToString());
  });
  SendDndLeave();

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  SendDndDropAndFinished();
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

  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(WaylandWindowDragController::State::kAttached,
            drag_controller_state());

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

  EXPECT_EQ(gfx::Point(0, 0), window_->GetBoundsInDIP().origin());
  EXPECT_EQ(gfx::Point(200, 200), screen_->GetCursorScreenPoint());

  // Set up an "interaction flow", start the drag session and run move loop:
  //  - Event dispatching and bounds changes are monitored
  //  - At each event, emulates a new event at server side and proceeds to the
  //  next test step.
  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);

  enum { kStarted, kDragging, kDropping, kDone } test_step = kStarted;

  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kStarted:
        EXPECT_EQ(EventType::kMouseEntered, event->type());
        EXPECT_EQ(State::kDetached, drag_controller_state());
        // Ensure PlatformScreen keeps consistent.
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({200, 200}, {}));
        // `window_` starts at origin 0,0.
        EXPECT_EQ(gfx::Point(0, 0), window_->GetBoundsInDIP().origin());
        // test_data_device sends first enter event at location 0,0.
        EXPECT_EQ(gfx::Point(0, 0), screen_->GetCursorScreenPoint());

        // Drag it a bit more. We are in the middle of wl_data_device.enter.
        // Run this via a task run. Otherwise, the data offer will be reset
        // and WaylandWindowDragController will crash.
        ScheduleTestTask(base::BindOnce(
            &WaylandWindowDragControllerTest::SendDndMotionForWindowDrag,
            base::Unretained(this), gfx::Point(50, 50)));
        test_step = kDragging;
        break;
      case kDropping: {
        if (event->type() == EventType::kMouseEntered) {
          EXPECT_EQ(window_.get(),
                    window_manager()->GetCurrentPointerFocusedWindow());
          return;
        }
        EXPECT_EQ(EventType::kMouseReleased, event->type());
        EXPECT_EQ(State::kDropped, drag_controller_state());

        // Ensure |window_|'s bounds did not change in response to 20,20
        // wl_pointer::motion events sent at |kDragging| test step.
        EXPECT_EQ(gfx::kNullAcceleratedWidget,
                  screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
        EXPECT_EQ(window_->GetWidget(),
                  screen_->GetLocalProcessWidgetAtPoint({100, 100}, {}));

        // TODO(crbug.com/40939396): Remove window's origin here once the
        // non-Lacros motion events handling in detached mode gets fixed.
        gfx::Point expected_point(50, 50);
        expected_point += window_->GetBoundsInDIP().origin().OffsetFromOrigin();
        EXPECT_EQ(expected_point, screen_->GetCursorScreenPoint());
        test_step = kDone;
      } break;
      case kDragging:
      default:
        FAIL() << " event=" << event->ToString()
               << " state=" << drag_controller_state()
               << " step=" << static_cast<int>(test_step);
    }
  });

  EXPECT_CALL(delegate_, OnBoundsChanged(_))
      .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
        EXPECT_EQ(State::kDetached, drag_controller_state());
        EXPECT_EQ(kDragging, test_step);
        EXPECT_TRUE(change.origin_changed);

        EXPECT_EQ(gfx::Point(50, 50), window_->GetBoundsInDIP().origin());
        EXPECT_EQ(gfx::Point(50, 50), screen_->GetCursorScreenPoint());

        // WaylandWindowDragController might be in the middle of something, when
        // calling this. However, given issuing commands to a server result in
        // running a RunLoop, the actual commands will result in events that the
        // client receives. That will result in a new flow while we are blocked
        // here, which means WaylandWindowDragController will not complete what
        // it must complete before getting the below commands processed by the
        // server and received by the client. Thus, prepare a task to avoid that
        // and let the WaylandWindowDragController to do what it needs to do.
        test_step = kDropping;
        ScheduleTestTask(base::BindLambdaForTesting([&]() {
          // Send a few wl_pointer::motion events. Required checks are done
          // in |kDropping| test step above.
          SendPointerMotion(nullptr, nullptr, gfx::Point(30, 30), false);
          SendPointerMotion(nullptr, nullptr, gfx::Point(20, 20), false);
          SendDndDropAndFinished();
        }));
      });

  // RunMoveLoop() blocks until the dragging session ends.
  EXPECT_TRUE(move_loop_handler->RunMoveLoop({}));

  // Verify expectations.
  Mock::VerifyAndClearExpectations(&delegate_);
  ASSERT_EQ(test_step, kDone);
  EXPECT_EQ(State::kIdle, drag_controller_state());
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
  GetWaylandToplevelExtension(*dragged_window)
      ->StartWindowDraggingSessionIfNeeded(DragEventSource::kMouse,
                                           /*allow_system_drag=*/false);
  WaylandTestBase::SyncDisplay();
  EXPECT_EQ(State::kAttached, drag_controller_state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*dragged_window);
  ASSERT_TRUE(move_loop_handler);

  auto test = [](WaylandWindowDragControllerTest* self,
                 WmMoveLoopHandler* move_loop_handler) {
    // While in |kDetached| state, motion events are expected to be propagated
    // by window drag controller as bounds changes.
    EXPECT_EQ(State::kDetached, self->drag_controller_state());
    EXPECT_CALL(self->delegate(), OnBoundsChanged(_))
        .WillOnce([&](const PlatformWindowDelegate::BoundsChange& change) {
          EXPECT_EQ(gfx::Point(30, 30),
                    self->window()->GetBoundsInDIP().origin());
          EXPECT_TRUE(change.origin_changed);
        });
    self->SendDndMotionForWindowDrag({30, 30});

    move_loop_handler->EndMoveLoop();

    // Otherwise, after the move loop is requested to quit, but before it really
    // ends (ie. kAttaching state), motion events are **not** expected to be
    // propagated.
    EXPECT_EQ(State::kAttaching, self->drag_controller_state());
    EXPECT_CALL(self->delegate(), OnBoundsChanged(_)).Times(0);
    self->SendDndMotionForWindowDrag({31, 31});
  };
  ScheduleTestTask(base::BindOnce(test, base::Unretained(this),
                                  base::Unretained(move_loop_handler)));

  // Spins move loop for |window_1|.
  // TODO(nickdiego): Should succeed for this test case.
  EXPECT_FALSE(move_loop_handler->RunMoveLoop({}));

  // When the transition to |kAttached| state is finally done (ie. nested loop
  // quits), motion events are then expected to be propagated by window drag
  // controller as usual.
  EXPECT_EQ(State::kAttached, drag_controller_state());
  EXPECT_CALL(delegate(), DispatchEvent(_)).Times(1);
  SendDndMotionForWindowDrag({30, 30});

  EXPECT_CALL(delegate(), DispatchEvent(_)).Times(1);
  SendDndDropAndFinished();

  SendPointerEnter(window_.get(), &delegate_);

  EXPECT_EQ(State::kIdle, drag_controller_state());
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

  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  EXPECT_CALL(delegate(), DispatchEvent(_)).Times(::testing::AtLeast(2));
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  WaylandTestBase::SyncDisplay();
  // Starting a DnD session results in a server sending a Enter event, which
  // enters the window at 0x0.
  EXPECT_EQ(gfx::Point(0, 0), screen_->GetCursorScreenPoint());
  EXPECT_EQ(State::kAttached, drag_controller_state());

  // Now move the pointer to 10x10 location and start the test.
  SendDndMotionForWindowDrag(p0);
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
      self->SendDndMotionForWindowDrag(p0);

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

      self->SendDndMotionForWindowDrag(p1);

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
  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);
  ScheduleTestTask(
      base::BindLambdaForTesting([&]() { move_loop_handler->EndMoveLoop(); }));

  // 3. Run the move loop. The client should receive the data_device.data_offer
  // and data_device.data_enter events before the drag is halted. Ensure these
  // server events arrive at the client before proceeding to ensure tests are
  // asserting on a consistent expected ordering of events.
  EXPECT_FALSE(move_loop_handler->RunMoveLoop({}));
  WaylandTestBase::SyncDisplay();

  // 4. Destroy the dragged window just after quitting move loop.
  const auto* dangling_window_ptr = window_.get();
  window_.reset();
  EXPECT_NE(dangling_window_ptr,
            TestApi(drag_controller()).pointer_grab_owner());
  EXPECT_EQ(State::kIdle, drag_controller_state());

  // 5. Ensure no events are dispatched for drop. Which indirectly means that
  // drop handling code at window drag controller does not call into the above
  // destroyed dragged window.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  TestApi(drag_controller()).EmulateOnDragDrop(EventTimeForNow());

  // 6. Verifies that related state is correctly reset after drop.
  EXPECT_EQ(State::kIdle, drag_controller_state());
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
  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());
  EXPECT_TRUE(drag_controller()->IsActiveDragAndDropSession());

  // 3. Spawns a new toplevel |window_2| out of the origin |window_|. Similarly
  // to when a tab is detached in a Chrome's tab drag session.
  PlatformWindowInitProperties properties{gfx::Rect{80, 80}};
  properties.type = PlatformWindowType::kWindow;
  MockWaylandPlatformWindowDelegate delegate_2;
  EXPECT_CALL(delegate_2, OnAcceleratedWidgetAvailable(_)).Times(1);
  auto window_2 =
      delegate_2.CreateWaylandWindow(connection_.get(), std::move(properties));
  ASSERT_NE(gfx::kNullAcceleratedWidget, window_2->GetWidget());
  window_2->Show(/*inactive=*/false);
  WaylandTestBase::SyncDisplay();

  // Spin the nested move loop and schedule a sequence of test steps to be
  // pefomed while it is running.
  ScheduleTestTask(base::BindLambdaForTesting([&]() {
    auto* cursor_tracker = connection_->wayland_cursor_position();
    ASSERT_TRUE(cursor_tracker);

    // Send a motion event and verify it is correctly propagated.
    EXPECT_CALL(delegate_2, OnBoundsChanged(_)).Times(1);
    SendDndMotionForWindowDrag({11, 10});
    EXPECT_EQ(gfx::Point(11, 10), cursor_tracker->GetCursorSurfacePoint());
    Mock::VerifyAndClearExpectations(&delegate_2);

    // Destroy the window being currently dragged. This will end the drag
    // session.
    window_2.reset();
    EXPECT_FALSE(drag_controller()->IsActiveDragAndDropSession());

    // Verify drag-and-drop events are not propagated after the drag ends.
    EXPECT_CALL(delegate_2, DispatchEvent(_)).Times(0);
    EXPECT_CALL(delegate_2, OnBoundsChanged(_)).Times(0);
    SendDndMotionForWindowDrag({12, 10});
    TestApi(drag_controller()).EmulateOnDragDrop(EventTimeForNow());
    Mock::VerifyAndClearExpectations(&delegate_2);

    // Verify that internal "last cursor position" is not updated after the
    // grab owner is destroyed.
    EXPECT_EQ(gfx::Point(11, 10), cursor_tracker->GetCursorSurfacePoint());
  }));

  ASSERT_TRUE(GetWmMoveLoopHandler(*window_2));
  EXPECT_FALSE(GetWmMoveLoopHandler(*window_2)->RunMoveLoop({}));

  // 4. Verifies that related state is correctly reset after the drag was
  // terminated. The remaining `window_` should remain under the pointer.
  EXPECT_EQ(State::kIdle, drag_controller_state());
  EXPECT_EQ(window_.get(),
            window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(window_->GetWidget(),
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));
}

// Ensures correct behavior when ext-drag protocol is not available, such as:
//
// 1. Returns 'success' even when wl_data_source.cancelled is sent by the
//    Wayland Compositor.
//
//  Regression test for https://crbug.com/1366504.
TEST_P(WaylandWindowDragControllerTest, ExtendedDragUnavailable) {
  ASSERT_TRUE(GetWmMoveLoopHandler(*window_));
  ASSERT_TRUE(GetWaylandToplevelExtension(*window_));
  TestApi(drag_controller()).set_extended_drag_available(false);

  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});

  GetWaylandToplevelExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());

  auto* move_loop_handler = GetWmMoveLoopHandler(*window_);
  ASSERT_TRUE(move_loop_handler);

  enum { kStarted, kDropping, kDone } test_step = kStarted;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly([&](Event* event) {
    EXPECT_TRUE(event->IsMouseEvent());
    switch (test_step) {
      case kStarted:
        EXPECT_EQ(EventType::kMouseEntered, event->type());
        EXPECT_EQ(State::kDetached, drag_controller_state());

        // We are in the middle of
        // WaylandWindowDragController::OnDragEnter. Run this via a task run.
        // Otherwise, the data offer will be reset and
        // WaylandWindowDragController will crash.
        ScheduleTestTask(base::BindOnce(&WaylandDragDropTest::SendDndCancelled,
                                        base::Unretained(this)));
        test_step = kDropping;
        break;
      case kDropping: {
        EXPECT_EQ(EventType::kMouseReleased, event->type());
        EXPECT_EQ(State::kDropped, drag_controller_state());

        test_step = kDone;
        break;
      }
      case kDone:
        EXPECT_EQ(EventType::kMouseExited, event->type());
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

  EXPECT_EQ(State::kIdle, drag_controller_state());
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
    auto serial =
        TestApi(drag_controller()).GetSerial(DragEventSource::kMouse, origin);
    EXPECT_FALSE(serial.has_value());
  }

  // Check cases where only pointer focus info is set.
  {  // Serial available, but no window focused.
    serial_tracker().UpdateSerial(wl::SerialType::kMousePress, 1u);
    auto serial =
        TestApi(drag_controller()).GetSerial(DragEventSource::kMouse, origin);
    EXPECT_FALSE(serial.has_value());
  }

  {  // Both serial and focused window available.
    window_manager.SetPointerFocusedWindow(window_.get());
    auto serial =
        TestApi(drag_controller()).GetSerial(DragEventSource::kMouse, origin);
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
    auto serial =
        TestApi(drag_controller()).GetSerial(DragEventSource::kTouch, origin);
    EXPECT_FALSE(serial.has_value());
  }
  {  // Both serial and focused window available.
    window_manager.SetTouchFocusedWindow(window_.get());
    auto serial =
        TestApi(drag_controller()).GetSerial(DragEventSource::kTouch, origin);
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
  GetWaylandToplevelExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);
  ASSERT_EQ(State::kIdle, drag_controller_state());
  ASSERT_FALSE(drag_controller()->drag_source().has_value());
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    ASSERT_FALSE(server->data_device_manager()->data_source());
  });
  SendPointerButton(window_.get(), &delegate_, BTN_LEFT, /*pressed=*/false);

  ASSERT_FALSE(serial_tracker().GetSerial(wl::SerialType::kTouchPress));

  // Now it should start successfully.
  GetWaylandToplevelExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);
  ASSERT_EQ(State::kIdle, drag_controller_state());
}

// Ensure events are handled appropriately when the target window is destroyed
// while the move loop is running (i.e. dragging in the detached state).
// Regression test for crbug.com/1433577.
TEST_P(WaylandWindowDragControllerTest,
       HandleTargetWindowDestruction_DetachedState) {
  // Send some initial pointer events to `window_`.
  ASSERT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});

  // Start the window drag session.
  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());
  EXPECT_TRUE(drag_controller()->IsActiveDragAndDropSession());

  // Spawns a new toplevel `window_2` out of the origin `window_`. Similarly to
  // when a tab is detached in a Chrome's tab drag session.
  PlatformWindowInitProperties properties{gfx::Rect{80, 80}};
  properties.type = PlatformWindowType::kWindow;
  MockWaylandPlatformWindowDelegate delegate_2;
  EXPECT_CALL(delegate_2, OnAcceleratedWidgetAvailable(_)).Times(1);
  auto window_2 =
      delegate_2.CreateWaylandWindow(connection_.get(), std::move(properties));
  ASSERT_NE(gfx::kNullAcceleratedWidget, window_2->GetWidget());
  window_2->Show(/*inactive=*/false);
  WaylandTestBase::SyncDisplay();

  // Spin the nested move loop and schedule a sequence of test steps to be
  // performed while it is running.
  ScheduleTestTask(base::BindLambdaForTesting([&]() {
    auto* cursor_tracker = connection_->wayland_cursor_position();
    ASSERT_TRUE(cursor_tracker);

    // Running the move loop will initiate dragging the window in the detached
    // state.
    EXPECT_EQ(State::kDetached, drag_controller_state());

    // Send a motion event and verify it is correctly propagated.
    EXPECT_CALL(delegate_2, OnBoundsChanged(_)).Times(1);
    SendDndMotionForWindowDrag({11, 10});
    EXPECT_EQ(gfx::Point(11, 10), cursor_tracker->GetCursorSurfacePoint());

    // Destroy the target window (which at this point should be the origin
    // window, grab owner and the target window).
    EXPECT_EQ(window_.get(), TestApi(drag_controller()).drag_target_window());
    EXPECT_EQ(window_.get(), TestApi(drag_controller()).pointer_grab_owner());
    EXPECT_EQ(window_.get(), TestApi(drag_controller()).origin_window());
    window_.reset();
    EXPECT_FALSE(drag_controller()->IsActiveDragAndDropSession());

    // Verify drag-and-drop events are not propagated for either the dragged
    // window or the origin windows after the drag ends.
    EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
    EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
    EXPECT_CALL(delegate_2, DispatchEvent(_)).Times(0);
    EXPECT_CALL(delegate_2, OnBoundsChanged(_)).Times(0);
    SendDndMotionForWindowDrag({12, 10});
    TestApi(drag_controller()).EmulateOnDragDrop(EventTimeForNow());

    // Verify that internal "last cursor position" is not updated after the
    // grab owner is destroyed.
    EXPECT_EQ(gfx::Point(11, 10), cursor_tracker->GetCursorSurfacePoint());
  }));

  ASSERT_TRUE(GetWmMoveLoopHandler(*window_2));
  EXPECT_FALSE(GetWmMoveLoopHandler(*window_2)->RunMoveLoop({}));

  // Verifies that related state is correctly reset.
  EXPECT_EQ(State::kIdle, drag_controller_state());
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

// Ensure events are handled appropriately when the target window is destroyed
// while dragging a tab attached to the window (i.e. dragging in the attached
// state).
// Regression test for crbug.com/1433577.
TEST_P(WaylandWindowDragControllerTest,
       HandleTargetWindowDestruction_AttachedState) {
  // Send some initial pointer events to |window_|.
  ASSERT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, {10, 10});

  // Start the window drag session.
  auto* wayland_extension = GetWaylandToplevelExtension(*window_);
  wayland_extension->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());
  EXPECT_TRUE(drag_controller()->IsActiveDragAndDropSession());

  // Move the tab around in the tab strip and validate the cursor is tracked
  // correctly.
  auto* cursor_tracker = connection_->wayland_cursor_position();
  ASSERT_TRUE(cursor_tracker);

  SendDndMotionForWindowDrag({11, 10});
  EXPECT_EQ(gfx::Point(11, 10), cursor_tracker->GetCursorSurfacePoint());
  SendDndMotionForWindowDrag({12, 10});
  EXPECT_EQ(gfx::Point(12, 10), cursor_tracker->GetCursorSurfacePoint());

  // Destroy the target window (which at this point should be the origin window,
  // grab owner and the target window).
  EXPECT_EQ(window_.get(), TestApi(drag_controller()).drag_target_window());
  EXPECT_EQ(window_.get(), TestApi(drag_controller()).pointer_grab_owner());
  EXPECT_EQ(window_.get(), TestApi(drag_controller()).origin_window());
  window_.reset();
  EXPECT_FALSE(drag_controller()->IsActiveDragAndDropSession());

  // Verify drag-and-drop events are not propagated after the drag session has
  // ended.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  SendDndMotionForWindowDrag({13, 10});
  TestApi(drag_controller()).EmulateOnDragDrop(EventTimeForNow());

  // Verifies that related state is correctly reset.
  EXPECT_EQ(State::kIdle, drag_controller_state());
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({20, 20}, {}));
}

TEST_P(WaylandWindowDragControllerTest,
       PointerReleaseBeforeServerAckCancellsDrag) {
  // Ensure there is no window currently focused.
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerMotion(window_.get(), &delegate_, /*location=*/{10, 10});

  // Start the drag session.
  GetWaylandToplevelExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());

  // Simulate a pointer release event arriving at the client before the server
  // has acknowledged the drag request. This should cancel the drag.
  pointer_delegate()->OnPointerButtonEvent(
      EventType::kMouseReleased, EF_LEFT_MOUSE_BUTTON, base::TimeTicks::Now(),
      window_.get(), wl::EventDispatchPolicy::kImmediate,
      /*allow_release_of_unpressed_button=*/false,
      /*is_synthesized=*/false);
  EXPECT_EQ(State::kIdle, drag_controller_state());
}

TEST_P(WaylandWindowDragControllerTest,
       TouchReleaseBeforeServerAckCancellsDrag) {
  // Ensure there is no window currently focused.
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            screen_->GetLocalProcessWidgetAtPoint({10, 10}, {}));

  SendTouchDown(window_.get(), &delegate_, /*id=*/0, /*location=*/{0, 0});
  SendTouchMotion(window_.get(), &delegate_, /*id=*/0, /*location=*/{10, 10});

  // Start the drag session.
  GetWaylandToplevelExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kTouch,
      /*allow_system_drag=*/false);
  EXPECT_EQ(State::kAttached, drag_controller_state());

  // Simulate a touch release event arriving at the client before the server
  // has acknowledged the drag request. This should cancel the drag.
  touch_delegate()->OnTouchReleaseEvent(base::TimeTicks::Now(), /*id=*/0,
                                        wl::EventDispatchPolicy::kImmediate,
                                        /*is_synthesized=*/false);
  EXPECT_EQ(State::kIdle, drag_controller_state());
}

// Regression test for b/336321329. Ensures all pressed mouse buttons are
// released when the window drag ends.
TEST_P(WaylandWindowDragControllerTest, AllPointersReleasedAfterDragEnd) {
  EXPECT_FALSE(window_manager()->GetCurrentPointerOrTouchFocusedWindow());

  // Dispatch enter and press events for several mouse buttons.
  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  SendPointerPress(window_.get(), &delegate_, BTN_RIGHT);
  SendPointerPress(window_.get(), &delegate_, BTN_MIDDLE);

  EXPECT_TRUE(pointer_delegate()->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(
      pointer_delegate()->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
  EXPECT_TRUE(
      pointer_delegate()->IsPointerButtonPressed(EF_MIDDLE_MOUSE_BUTTON));

  // Start a drag with multiple mouse buttons pressed.
  GetWaylandToplevelExtension(*window_)->StartWindowDraggingSessionIfNeeded(
      DragEventSource::kMouse,
      /*allow_system_drag=*/false);
  ASSERT_EQ(State::kAttached, drag_controller_state());

  // End the drag, all pressed mouse buttons should have been released.
  SendDndDropAndFinished();
  EXPECT_EQ(State::kIdle, drag_controller_state());

  EXPECT_FALSE(
      pointer_delegate()->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate()->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate()->IsPointerButtonPressed(EF_MIDDLE_MOUSE_BUTTON));
}

// Regression test for crbug.com/330274075. There are circumstances under which
// compositors will not send a send data_source.dnd_finish|cancelled for a
// wayland drag session. This can result in a data drag leaving the shared state
// in the data device in an inconsistent state. If a window drag session is
// requested while in such an inconsistent state this shared state must first be
// reset.
TEST_P(WaylandWindowDragControllerTest, OutgoingSessionWithoutDndFinished) {
  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);

  // Once the drag session effectively starts at server-side, emulate a
  // data_source.dnd_drop_performed without its subsequent dnd_finished.
  ScheduleTestTask(
      base::BindLambdaForTesting([&]() { SendDndDropPerformed(); }));

  // Start the data drag session, which spins a nested message loop, and ensure
  // it quits even without wl_data_source.dnd_finished. In which case, the
  // expected side effect is drag controller's internal state left inconsistent,
  // ie: not reset to `kIdle`.
  OSExchangeData os_exchange_data;
  os_exchange_data.SetString(u"dnd-data");
  base::MockOnceCallback<void()> start_callback;
  base::MockOnceCallback<void(mojom::DragOperation)> completion_callback;
  window_->StartDrag(os_exchange_data,
                     DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE,
                     DragEventSource::kMouse, /*cursor=*/{},
                     /*can_grab_pointer=*/true, start_callback.Get(),
                     completion_callback.Get(),
                     /*loation delegate=*/nullptr);
  EXPECT_NE(connection_->data_drag_controller()->state_,
            WaylandDataDragController::State::kIdle);

  // Attempt to start a window drag with the left mouse button. It should
  // succeed despite data device state not having been reset correctly.
  SendPointerEnter(window_.get(), &delegate_);
  SendPointerPress(window_.get(), &delegate_, BTN_LEFT);
  EXPECT_TRUE(drag_controller()->StartDragSession(
      window_->AsWaylandToplevelWindow(), DragEventSource::kMouse));
  EXPECT_EQ(State::kAttached, drag_controller_state());

  // End the drag.
  SendDndDropAndFinished();
  EXPECT_EQ(State::kIdle, drag_controller_state());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Lacros requires aura shell.
INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandWindowDragControllerTest,
                         Values(wl::ServerConfig{}));
#else
// Linux shouldn't use aura shell.
INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestWithAuraShell,
    WaylandWindowDragControllerTest,
    Values(wl::ServerConfig{
        .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled}));
#endif

}  // namespace ui
