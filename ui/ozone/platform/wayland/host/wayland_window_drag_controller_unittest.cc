// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input-event-codes.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include <cstdint>

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
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"

using testing::_;
using testing::Mock;

namespace ui {

class WaylandWindowDragControllerTest : public WaylandTest,
                                        public wl::TestDataDevice::Delegate {
 public:
  WaylandWindowDragControllerTest() = default;
  ~WaylandWindowDragControllerTest() override = default;

  void SetUp() override {
    WaylandTest::SetUp();
    screen_ = std::make_unique<WaylandScreen>(connection_.get());

    wl_seat_send_capabilities(server_.seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
    Sync();
    pointer_ = server_.seat()->pointer();
    ASSERT_TRUE(pointer_);

    EXPECT_FALSE(window_->has_pointer_focus());
    EXPECT_EQ(State::kIdle, drag_controller()->state());

    data_device_manager_ = server_.data_device_manager();
    DCHECK(data_device_manager_);

    source_ = nullptr;
    data_device_manager_->data_device()->set_delegate(this);
  }

  void TearDown() override {
    data_device_manager_->data_device()->set_delegate(nullptr);
  }

  WaylandWindowDragController* drag_controller() const {
    return connection_->window_drag_controller();
  }

  WaylandWindowManager* window_manager() const {
    return connection_->wayland_window_manager();
  }

  uint32_t NextSerial() const {
    static uint32_t serial = 0;
    return ++serial;
  }

  uint32_t NextTime() const {
    static uint32_t timestamp = 0;
    return ++timestamp;
  }

 protected:
  using State = WaylandWindowDragController::State;

  // wl::TestDataDevice::Delegate:
  void StartDrag(wl::TestDataSource* source,
                 wl::MockSurface* origin,
                 uint32_t serial) override {
    EXPECT_FALSE(source_);
    source_ = source;
    OfferAndEnter(origin);
  }

  // Helper functions
  void SendDndMotion(const gfx::Point& location) {
    EXPECT_TRUE(source_);
    wl_fixed_t x = wl_fixed_from_int(location.x());
    wl_fixed_t y = wl_fixed_from_int(location.y());
    data_device_manager_->data_device()->OnMotion(NextTime(), x, y);
  }

  void SendDndEnter(WaylandWindow* window) {
    EXPECT_TRUE(window);
    OfferAndEnter(server_.GetObject<wl::MockSurface>(
        window->root_surface()->GetSurfaceId()));
  }

  void SendDndLeave() {
    EXPECT_TRUE(source_);
    data_device_manager_->data_device()->OnLeave();
  }

  void SendDndDrop() {
    EXPECT_TRUE(source_);
    source_->OnCancelled();
  }

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
                         gfx::Point location) {
    wl_fixed_t x = wl_fixed_from_int(location.x());
    wl_fixed_t y = wl_fixed_from_int(location.y());
    wl_pointer_send_motion(pointer_->resource(), NextTime(), x, y);
    EXPECT_CALL(*delegate, DispatchEvent(_)).WillOnce([](Event* event) {
      EXPECT_TRUE(event->IsMouseEvent());
      EXPECT_EQ(ET_MOUSE_DRAGGED, event->type());
    });
    Sync();

    EXPECT_EQ(window->GetWidget(),
              screen_->GetLocalProcessWidgetAtPoint(location, {}));
  }

  void OfferAndEnter(wl::MockSurface* surface) {
    EXPECT_TRUE(source_);
    auto* data_device = data_device_manager_->data_device();
    auto* offer = data_device->OnDataOffer();
    EXPECT_EQ(1u, source_->mime_types().size());
    for (const auto& mime_type : source_->mime_types())
      offer->OnOffer(mime_type, {});

    wl_data_device_send_enter(data_device->resource(), NextSerial(),
                              surface->resource(), 0, 0, offer->resource());
  }

  // client objects
  std::unique_ptr<WaylandScreen> screen_;

  // server objects
  wl::TestDataDeviceManager* data_device_manager_;
  wl::TestDataSource* source_;
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
        SendDndEnter(target_window);
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

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandWindowDragControllerTest,
                         ::testing::Values(kXdgShellStable));

INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandWindowDragControllerTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
