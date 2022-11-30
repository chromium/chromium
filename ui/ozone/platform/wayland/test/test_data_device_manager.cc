// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"

#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

namespace wl {

namespace {

constexpr uint32_t kDataDeviceManagerVersion = 3;

void CreateDataSource(wl_client* client, wl_resource* resource, uint32_t id) {
  CreateResourceWithImpl<TestDataSource>(client, &wl_data_source_interface,
                                         wl_resource_get_version(resource),
                                         &kTestDataSourceImpl, id);
}

void GetDataDevice(wl_client* client,
                   wl_resource* manager_resource,
                   uint32_t id,
                   wl_resource* seat_resource) {
  auto* manager = GetUserDataAs<TestDataDeviceManager>(manager_resource);
  CHECK(manager);

  wl_resource* resource = CreateResourceWithImpl<TestDataDevice>(
      client, &wl_data_device_interface,
      wl_resource_get_version(manager_resource), &kTestDataDeviceImpl, id,
      manager);

  CHECK(GetUserDataAs<TestDataDevice>(resource));
  manager->set_data_device(GetUserDataAs<TestDataDevice>(resource));
}

}  // namespace

const struct wl_data_device_manager_interface kTestDataDeviceManagerImpl = {
    &CreateDataSource, &GetDataDevice};

TestDataDeviceManager::TestDataDeviceManager()
    : GlobalObject(&wl_data_device_manager_interface,
                   &kTestDataDeviceManagerImpl,
                   kDataDeviceManagerVersion) {}

TestDataDeviceManager::~TestDataDeviceManager() = default;

}  // namespace wl
