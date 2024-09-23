// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_data_device.h"

#include <wayland-server-core.h>

#include <cstdint>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"

namespace wl {

namespace {

void DataDeviceStartDrag(wl_client* client,
                         wl_resource* resource,
                         wl_resource* source,
                         wl_resource* origin,
                         wl_resource* icon,
                         uint32_t serial) {
  auto* data_source = GetUserDataAs<TestDataSource>(source);
  auto* origin_surface = GetUserDataAs<MockSurface>(origin);

  GetUserDataAs<TestDataDevice>(resource)->StartDrag(data_source,
                                                     origin_surface, serial);
}

void DataDeviceRelease(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

struct WlDataDeviceImpl : public TestSelectionDevice::Delegate {
  explicit WlDataDeviceImpl(TestDataDevice* device) : device_(device) {}
  ~WlDataDeviceImpl() override = default;

  WlDataDeviceImpl(const WlDataDeviceImpl&) = delete;
  WlDataDeviceImpl& operator=(const WlDataDeviceImpl&) = delete;

  TestSelectionOffer* CreateAndSendOffer() override {
    wl_resource* device_resource = device_->resource();
    wl_resource* new_offer_resource = CreateResourceWithImpl<TestDataOffer>(
        wl_resource_get_client(device_resource), &wl_data_offer_interface,
        wl_resource_get_version(device_resource), &kTestDataOfferImpl, 0);
    wl_data_device_send_data_offer(device_resource, new_offer_resource);
    return GetUserDataAs<TestSelectionOffer>(new_offer_resource);
  }

  void HandleSetSelection(TestSelectionSource* source,
                          uint32_t serial) override {
    device_->SetSelection(static_cast<TestDataSource*>(source), serial);
  }

  void SendSelection(TestSelectionOffer* selection_offer) override {
    CHECK(selection_offer);
    wl_data_device_send_selection(device_->resource(),
                                  selection_offer->resource());
  }

 private:
  const raw_ptr<TestDataDevice> device_;
};

}  // namespace

const struct wl_data_device_interface kTestDataDeviceImpl = {
    &DataDeviceStartDrag, &TestSelectionDevice::SetSelection,
    &DataDeviceRelease};

TestDataDevice::TestDataDevice(wl_resource* resource,
                               TestDataDeviceManager* manager)
    : TestSelectionDevice(resource, std::make_unique<WlDataDeviceImpl>(this)),
      manager_(manager) {}

TestDataDevice::~TestDataDevice() = default;

void TestDataDevice::SetSelection(TestDataSource* data_source,
                                  uint32_t serial) {
  CHECK(manager_);
  manager_->set_data_source(data_source);
}

TestDataOffer* TestDataDevice::CreateAndSendDataOffer() {
  return static_cast<TestDataOffer*>(TestSelectionDevice::OnDataOffer());
}

void TestDataDevice::StartDrag(TestDataSource* source,
                               MockSurface* origin,
                               uint32_t serial) {
  DCHECK(source);
  DCHECK(origin);

  CHECK(manager_);
  drag_serial_ = serial;
  manager_->set_data_source(source);
  if (auto_send_start_drag_events_) {
    SendOfferAndEnter(origin, {});
  }
  auto_send_start_drag_events_ = true;
  wl_client_flush(wl_resource_get_client(resource()));
}

void TestDataDevice::SendOfferAndEnter(MockSurface* origin,
                                       const gfx::Point& location) {
  DCHECK(manager_->data_source());
  auto* data_offer = OnDataOffer();
  DCHECK(data_offer);
  for (const auto& mime_type : manager_->data_source()->mime_types())
    data_offer->OnOffer(mime_type, {});

  auto* client = wl_resource_get_client(resource());
  auto* display = wl_client_get_display(client);
  DCHECK(display);
  wl_data_device_send_enter(resource(), wl_display_get_serial(display),
                            origin->resource(), wl_fixed_from_int(location.x()),
                            wl_fixed_from_int(location.y()),
                            data_offer->resource());
}

void TestDataDevice::OnEnter(uint32_t serial,
                             wl_resource* surface,
                             wl_fixed_t x,
                             wl_fixed_t y,
                             TestDataOffer* data_offer) {
  wl_data_device_send_enter(resource(), serial, surface, x, y,
                            data_offer->resource());
}

void TestDataDevice::OnLeave() {
  wl_data_device_send_leave(resource());
}

void TestDataDevice::OnMotion(uint32_t time, wl_fixed_t x, wl_fixed_t y) {
  wl_data_device_send_motion(resource(), time, x, y);
}

void TestDataDevice::OnDrop() {
  wl_data_device_send_drop(resource());
}

}  // namespace wl
