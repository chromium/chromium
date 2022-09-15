// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/wayland_drag_drop_test.h"

#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include <cstdint>

#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"

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
  auto* surface = server_.GetObject<wl::MockSurface>(
      window->root_surface()->GetSurfaceId());
  OfferAndEnter(surface, location);
}

void WaylandDragDropTest::SendDndLeave() {
  data_device_manager_->data_device()->OnLeave();
}

void WaylandDragDropTest::SendDndMotion(const gfx::Point& location) {
  EXPECT_TRUE(data_source_);
  wl_fixed_t x = wl_fixed_from_int(location.x());
  wl_fixed_t y = wl_fixed_from_int(location.y());
  data_device_manager_->data_device()->OnMotion(NextTime(), x, y);
}

void WaylandDragDropTest::SendDndDrop() {
  EXPECT_TRUE(data_source_);
  data_source_->OnFinished();
}

void WaylandDragDropTest::SendDndCancelled() {
  EXPECT_TRUE(data_source_);
  data_source_->OnCancelled();
}

void WaylandDragDropTest::SendDndAction(uint32_t action) {
  EXPECT_TRUE(data_source_);
  data_source_->OnDndAction(action);
}

void WaylandDragDropTest::ReadData(
    const std::string& mime_type,
    wl::TestDataSource::ReadDataCallback callback) {
  ASSERT_TRUE(data_source_);
  data_source_->ReadData(mime_type, std::move(callback));
}

void WaylandDragDropTest::SendPointerEnter(
    WaylandWindow* window,
    MockPlatformWindowDelegate* delegate) {
  auto* surface = server_.GetObject<wl::MockSurface>(
      window->root_surface()->GetSurfaceId());
  wl_pointer_send_enter(pointer_->resource(), NextSerial(), surface->resource(),
                        0, 0);
  wl_pointer_send_frame(pointer_->resource());
}

void WaylandDragDropTest::SendPointerLeave(
    WaylandWindow* window,
    MockPlatformWindowDelegate* delegate) {
  auto* surface = server_.GetObject<wl::MockSurface>(
      window->root_surface()->GetSurfaceId());
  wl_pointer_send_leave(pointer_->resource(), NextSerial(),
                        surface->resource());
  wl_pointer_send_frame(pointer_->resource());
}

void WaylandDragDropTest::SendPointerButton(
    WaylandWindow* window,
    MockPlatformWindowDelegate* delegate,
    int button,
    bool pressed) {
  const uint32_t serial = NextSerial();
  uint32_t state = pressed ? WL_POINTER_BUTTON_STATE_PRESSED
                           : WL_POINTER_BUTTON_STATE_RELEASED;
  wl_pointer_send_button(pointer_->resource(), serial, NextTime(), button,
                         state);
  wl_pointer_send_frame(pointer_->resource());
}

void WaylandDragDropTest::SendTouchDown(WaylandWindow* window,
                                        MockPlatformWindowDelegate* delegate,
                                        int id,
                                        const gfx::Point& location) {
  auto* surface = server_.GetObject<wl::MockSurface>(
      window->root_surface()->GetSurfaceId());
  wl_touch_send_down(
      touch_->resource(), NextSerial(), NextTime(), surface->resource(), id,
      wl_fixed_from_double(location.x()), wl_fixed_from_double(location.y()));
  wl_touch_send_frame(touch_->resource());
}

void WaylandDragDropTest::SendTouchUp(int id) {
  wl_touch_send_up(touch_->resource(), NextSerial(), NextTime(), id);
  wl_touch_send_frame(touch_->resource());
}

void WaylandDragDropTest::SendTouchMotion(WaylandWindow* window,
                                          MockPlatformWindowDelegate* delegate,
                                          int id,
                                          const gfx::Point& location) {
  wl_touch_send_motion(touch_->resource(), NextSerial(), id,
                       wl_fixed_from_double(location.x()),
                       wl_fixed_from_double(location.y()));
  wl_touch_send_frame(touch_->resource());
}

void WaylandDragDropTest::SetUp() {
  WaylandTest::SetUp();

  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER |
                                WL_SEAT_CAPABILITY_TOUCH |
                                WL_SEAT_CAPABILITY_KEYBOARD);

  Sync();
  pointer_ = server_.seat()->pointer();
  ASSERT_TRUE(pointer_);

  touch_ = server_.seat()->touch();
  ASSERT_TRUE(touch_);

  data_device_manager_ = server_.data_device_manager();
  ASSERT_TRUE(data_device_manager_);

  data_source_ = nullptr;
  data_device_manager_->data_device()->set_drag_delegate(this);
}

void WaylandDragDropTest::TearDown() {
  data_device_manager_->data_device()->set_drag_delegate(nullptr);
  data_device_manager_ = nullptr;
}

// wl::TestDataDevice::DragDelegate:
void WaylandDragDropTest::StartDrag(wl::TestDataSource* source,
                                    wl::MockSurface* origin,
                                    uint32_t serial) {
  EXPECT_FALSE(data_source_);
  data_source_ = source;
  OfferAndEnter(origin, {});
  MockStartDrag(source, origin, serial);
}

uint32_t WaylandDragDropTest::NextSerial() {
  static uint32_t serial = 0;
  current_serial_ = ++serial;
  return current_serial_;
}

uint32_t WaylandDragDropTest::NextTime() const {
  static uint32_t timestamp = 0;
  return ++timestamp;
}

void WaylandDragDropTest::OfferAndEnter(wl::MockSurface* surface,
                                        const gfx::Point& location) {
  ASSERT_TRUE(data_source_);
  auto* data_device = data_device_manager_->data_device();

  // Emulate server sending an wl_data_device::offer event.
  auto* data_offer = data_device->OnDataOffer();
  for (const auto& mime_type : data_source_->mime_types())
    data_offer->OnOffer(mime_type, {});

  // Emulate server sending an wl_data_device::enter event.
  wl_data_device_send_enter(
      data_device->resource(), NextSerial(), surface->resource(),
      wl_fixed_from_int(location.x()), wl_fixed_from_int(location.y()),
      data_offer->resource());
}

void WaylandDragDropTest::ScheduleTestTask(base::OnceClosure test_task) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&WaylandDragDropTest::RunTestTask,
                                base::Unretained(this), std::move(test_task)));
}

void WaylandDragDropTest::RunTestTask(base::OnceClosure test_task) {
  Sync();

  // The data source is created asynchronously by the drag controller. If it is
  // null at this point, it means that the task for that has not yet executed,
  // so try again a bit later.
  if (!data_device_manager_->data_source()) {
    ScheduleTestTask(std::move(test_task));
    return;
  }

  std::move(test_task).Run();
}

}  // namespace ui
