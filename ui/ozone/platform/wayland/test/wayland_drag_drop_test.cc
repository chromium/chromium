// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/wayland_drag_drop_test.h"

#include <wayland-util.h>

#include <cstdint>

#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"

namespace ui {

WaylandDragDropTest::WaylandDragDropTest() = default;

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

void WaylandDragDropTest::SendDndCancelled() {
  EXPECT_TRUE(data_source_);
  data_source_->OnCancelled();
}

void WaylandDragDropTest::ReadData(
    const std::string& mime_type,
    wl::TestDataSource::ReadDataCallback callback) {
  ASSERT_TRUE(data_source_);
  data_source_->ReadData(mime_type, std::move(callback));
}

void WaylandDragDropTest::SetUp() {
  WaylandTest::SetUp();
  Sync();

  data_device_manager_ = server_.data_device_manager();
  ASSERT_TRUE(data_device_manager_);

  data_source_ = nullptr;
  data_device_manager_->data_device()->set_delegate(this);
}

void WaylandDragDropTest::TearDown() {
  data_device_manager_->data_device()->set_delegate(nullptr);
  data_device_manager_ = nullptr;
}

// wl::TestDataDevice::Delegate:
void WaylandDragDropTest::StartDrag(wl::TestDataSource* source,
                                    wl::MockSurface* origin,
                                    uint32_t serial) {
  EXPECT_FALSE(data_source_);
  data_source_ = source;
  OfferAndEnter(origin, {});
}

uint32_t WaylandDragDropTest::NextSerial() const {
  static uint32_t serial = 0;
  return ++serial;
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
