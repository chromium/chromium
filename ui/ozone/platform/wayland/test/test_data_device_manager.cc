// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"

#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"

namespace wl {

namespace {

constexpr uint32_t kDataDeviceManagerVersion = 3;

void CreateDataSource(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* data_source_resource = CreateResourceWithImpl<TestDataSource>(
      client, &wl_data_source_interface, wl_resource_get_version(resource),
      &kTestDataSourceImpl, id);
  GetUserDataAs<TestDataDeviceManager>(resource)->set_data_source(
      GetUserDataAs<TestDataSource>(data_source_resource));
}

void GetDataDevice(wl_client* client,
                   wl_resource* data_device_manager_resource,
                   uint32_t id,
                   wl_resource* seat_resource) {
  wl_resource* resource = CreateResourceWithImpl<TestDataDevice>(
      client, &wl_data_device_interface,
      wl_resource_get_version(data_device_manager_resource),
      &kTestDataDeviceImpl, id, client);
  GetUserDataAs<TestDataDeviceManager>(data_device_manager_resource)
      ->set_data_device(GetUserDataAs<TestDataDevice>(resource));
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
