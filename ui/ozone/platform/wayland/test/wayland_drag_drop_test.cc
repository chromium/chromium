// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/wayland_drag_drop_test.h"

#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include <cstdint>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"

using testing::_;

namespace ui {

TestWaylandOSExchangeDataProvideFactory::
    TestWaylandOSExchangeDataProvideFactory() {
  SetInstance(this);
}

TestWaylandOSExchangeDataProvideFactory::
    ~TestWaylandOSExchangeDataProvideFactory() {
  SetInstance(nullptr);
}

std::unique_ptr<OSExchangeDataProvider>
TestWaylandOSExchangeDataProvideFactory::CreateProvider() {
  return std::make_unique<WaylandExchangeDataProvider>();
}

WaylandDragDropTest::WaylandDragDropTest() = default;

WaylandDragDropTest::~WaylandDragDropTest() = default;

void WaylandDragDropTest::SendDndEnter(WaylandWindow* window,
                                       const gfx::Point& location) {
  const uint32_t surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait(
      [surface_id, location](wl::TestWaylandServerThread* server) {
        auto* origin = server->GetObject<wl::MockSurface>(surface_id);
        ASSERT_TRUE(origin);
        auto* data_device = server->data_device_manager()->data_device();
        ASSERT_TRUE(data_device);
        data_device->SendOfferAndEnter(origin, location);
      });
}

void WaylandDragDropTest::SendDndLeave() {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->data_device_manager()->data_device()->OnLeave();
  });
}

void WaylandDragDropTest::SendDndMotion(const gfx::Point& location) {
  PostToServerAndWait([location](wl::TestWaylandServerThread* server) {
    auto* data_source = server->data_device_manager()->data_source();
    ASSERT_TRUE(data_source);
    wl_fixed_t x = wl_fixed_from_int(location.x());
    wl_fixed_t y = wl_fixed_from_int(location.y());
    server->data_device_manager()->data_device()->OnMotion(
        server->GetNextTime(), x, y);
  });
}

void WaylandDragDropTest::SendDndFinished() {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* data_source = server->data_device_manager()->data_source();
    ASSERT_TRUE(data_source);
    data_source->OnFinished();
  });
}

void WaylandDragDropTest::SendDndDropPerformed() {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* data_source = server->data_device_manager()->data_source();
    ASSERT_TRUE(data_source);
    data_source->OnDndDropPerformed();
  });
}

void WaylandDragDropTest::SendDndCancelled() {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* data_source = server->data_device_manager()->data_source();
    ASSERT_TRUE(data_source);
    data_source->OnCancelled();
  });
}

void WaylandDragDropTest::SendDndAction(uint32_t action) {
  PostToServerAndWait([action](wl::TestWaylandServerThread* server) {
    auto* data_source = server->data_device_manager()->data_source();
    ASSERT_TRUE(data_source);
    data_source->OnDndAction(action);
  });
}

void WaylandDragDropTest::ReadAndCheckData(const std::string& mime_type,
                                           const std::string& expected_data) {
  PostToServerAndWait([mime_type,
                       expected_data](wl::TestWaylandServerThread* server) {
    auto* server_data_source = server->data_device_manager()->data_source();
    ASSERT_TRUE(server_data_source);

    // Data fetching is done asynchronously using wl_display.sync callbacks,
    // so a nested run loop is required to ensure it is fully and reliably done.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    server_data_source->ReadData(
        mime_type, base::BindLambdaForTesting([&run_loop, &expected_data](
                                                  std::vector<uint8_t>&& data) {
          std::string result(data.begin(), data.end());
          EXPECT_EQ(expected_data, result);
          run_loop.Quit();
        }));
    run_loop.Run();
  });
}

void WaylandDragDropTest::SendPointerEnter(
    WaylandWindow* window,
    MockPlatformWindowDelegate* delegate) {
  const uint32_t surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id);
    ASSERT_TRUE(surface);
    auto* pointer = server->seat()->pointer();
    ASSERT_TRUE(pointer);
    wl_pointer_send_enter(pointer->resource(), server->GetNextSerial(),
                          surface->resource(), 0, 0);
    wl_pointer_send_frame(pointer->resource());
  });
}

void WaylandDragDropTest::SendPointerLeave(
    WaylandWindow* window,
    MockPlatformWindowDelegate* delegate) {
  const uint32_t surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* surface = server->GetObject<wl::MockSurface>(surface_id);
    ASSERT_TRUE(surface);
    auto* pointer = server->seat()->pointer();
    ASSERT_TRUE(pointer);
    wl_pointer_send_leave(pointer->resource(), server->GetNextSerial(),
                          surface->resource());
    wl_pointer_send_frame(pointer->resource());
  });
}

void WaylandDragDropTest::SendPointerButton(
    WaylandWindow* window,
    MockPlatformWindowDelegate* delegate,
    int button,
    bool pressed) {
  PostToServerAndWait([pressed, button](wl::TestWaylandServerThread* server) {
    uint32_t state = pressed ? WL_POINTER_BUTTON_STATE_PRESSED
                             : WL_POINTER_BUTTON_STATE_RELEASED;
    auto* pointer = server->seat()->pointer();
    ASSERT_TRUE(pointer);
    wl_pointer_send_button(pointer->resource(), server->GetNextSerial(),
                           server->GetNextTime(), button, state);
    wl_pointer_send_frame(pointer->resource());
  });
}

void WaylandDragDropTest::SendTouchDown(WaylandWindow* window,
                                        MockPlatformWindowDelegate* delegate,
                                        int id,
                                        const gfx::Point& location) {
  const uint32_t surface_id = window->root_surface()->get_surface_id();
  PostToServerAndWait(
      [surface_id, id, location](wl::TestWaylandServerThread* server) {
        auto* surface = server->GetObject<wl::MockSurface>(surface_id);
        ASSERT_TRUE(surface);
        auto* touch = server->seat()->touch();
        ASSERT_TRUE(touch);
        wl_touch_send_down(touch->resource(), server->GetNextSerial(),
                           server->GetNextTime(), surface->resource(), id,
                           wl_fixed_from_double(location.x()),
                           wl_fixed_from_double(location.y()));
        wl_touch_send_frame(touch->resource());
      });
}

void WaylandDragDropTest::SendTouchUp(int id) {
  PostToServerAndWait([id](wl::TestWaylandServerThread* server) {
    auto* touch = server->seat()->touch();
    ASSERT_TRUE(touch);
    wl_touch_send_up(touch->resource(), server->GetNextSerial(),
                     server->GetNextTime(), id);
    wl_touch_send_frame(touch->resource());
  });
}

void WaylandDragDropTest::SendTouchMotion(WaylandWindow* window,
                                          MockPlatformWindowDelegate* delegate,
                                          int id,
                                          const gfx::Point& location) {
  PostToServerAndWait([id, location](wl::TestWaylandServerThread* server) {
    auto* touch = server->seat()->touch();
    ASSERT_TRUE(touch);
    wl_touch_send_motion(touch->resource(), server->GetNextSerial(), id,
                         wl_fixed_from_double(location.x()),
                         wl_fixed_from_double(location.y()));
    wl_touch_send_frame(touch->resource());
  });
}

void WaylandDragDropTest::SetUp() {
  WaylandTest::SetUp();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER |
                                  WL_SEAT_CAPABILITY_TOUCH |
                                  WL_SEAT_CAPABILITY_KEYBOARD);
    ASSERT_TRUE(server->data_device_manager());
  });

  ASSERT_TRUE(connection_->seat());
  ASSERT_TRUE(connection_->seat()->pointer());
  ASSERT_TRUE(connection_->seat()->touch());
  ASSERT_TRUE(connection_->seat()->keyboard());
}

void WaylandDragDropTest::ScheduleTestTask(base::OnceClosure test_task) {
  scheduled_tasks_.emplace_back(std::move(test_task));
  MaybeRunScheduledTasks();
}

void WaylandDragDropTest::MaybeRunScheduledTasks() {
  if (is_task_running_ || scheduled_tasks_.empty()) {
    return;
  }

  is_task_running_ = true;

  auto next_task = std::move(scheduled_tasks_.front());
  scheduled_tasks_.erase(scheduled_tasks_.begin());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WaylandDragDropTest::RunTestTask,
                                base::Unretained(this), std::move(next_task)));
}

void WaylandDragDropTest::RunTestTask(base::OnceClosure test_task) {
  ASSERT_TRUE(is_task_running_);
  std::move(test_task).Run();
  is_task_running_ = false;
  MaybeRunScheduledTasks();
}

}  // namespace ui
