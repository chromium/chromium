// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

#include <wayland-server-core.h>

#include "base/check.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

void Destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

}  // namespace

const struct zwp_primary_selection_device_manager_v1_interface
    TestSelectionDeviceManager::kTestSelectionManagerImpl = {
        &CreateSource, &GetDevice, &Destroy};

// TODO(crbug.com/1204670): Implement primary selection offer.
struct TestSelectionOffer : public ServerObject {
  explicit TestSelectionOffer(wl_resource* resource) : ServerObject(resource) {}
  ~TestSelectionOffer() override = default;
};

TestSelectionSource::TestSelectionSource(wl_resource* resource)
    : ServerObject(resource) {}

TestSelectionSource::~TestSelectionSource() = default;

// TODO(crbug.com/1204670): Implement primary selection source.
void TestSelectionSource::Offer(struct wl_client* client,
                                struct wl_resource* resource,
                                const char* mime_type) {
  NOTIMPLEMENTED();
}

TestSelectionDevice::TestSelectionDevice(wl_resource* resource,
                                         wl_client* client)
    : ServerObject(resource) {}

TestSelectionDevice::~TestSelectionDevice() = default;

void TestSelectionDevice::SendSelectionOffer(
    const ui::PlatformClipboard::DataMap& data_map) {
  NOTIMPLEMENTED();
}

// TODO(crbug.com/1204670): Implement primary selection device.
void TestSelectionDevice::SetSelection(struct wl_client* client,
                                       struct wl_resource* resource,
                                       struct wl_resource* source,
                                       uint32_t serial) {
  NOTIMPLEMENTED();
}

TestSelectionDeviceManager::TestSelectionDeviceManager()
    : GlobalObject(&zwp_primary_selection_device_manager_v1_interface,
                   &kTestSelectionManagerImpl,
                   1) {}

TestSelectionDeviceManager::~TestSelectionDeviceManager() = default;

void TestSelectionDeviceManager::CreateSource(wl_client* client,
                                              wl_resource* manager_resource,
                                              uint32_t id) {
  const struct zwp_primary_selection_source_v1_interface
      kTestSelectionSourceImpl = {&TestSelectionSource::Offer, &Destroy};
  wl_resource* source_resource = CreateResourceWithImpl<TestSelectionSource>(
      client, &zwp_primary_selection_source_v1_interface,
      wl_resource_get_version(manager_resource), &kTestSelectionSourceImpl, id);

  auto* manager = GetUserDataAs<TestSelectionDeviceManager>(manager_resource);
  CHECK(manager);
  manager->source_ = GetUserDataAs<TestSelectionSource>(source_resource);
}

void TestSelectionDeviceManager::GetDevice(wl_client* client,
                                           wl_resource* manager_resource,
                                           uint32_t id,
                                           wl_resource* seat_resource) {
  const struct zwp_primary_selection_device_v1_interface
      kTestSelectionDeviceImpl = {&TestSelectionDevice::SetSelection, &Destroy};
  wl_resource* resource = CreateResourceWithImpl<TestSelectionDevice>(
      client, &zwp_primary_selection_device_v1_interface,
      wl_resource_get_version(manager_resource), &kTestSelectionDeviceImpl, id,
      client);

  auto* manager = GetUserDataAs<TestSelectionDeviceManager>(manager_resource);
  CHECK(manager);
  manager->device_ = GetUserDataAs<TestSelectionDevice>(resource);
}

}  // namespace wl
