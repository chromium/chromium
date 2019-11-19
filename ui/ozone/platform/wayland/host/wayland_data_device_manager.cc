// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

namespace ui {

WaylandDataDeviceManager::WaylandDataDeviceManager(
    wl_data_device_manager* device_manager,
    WaylandConnection* connection)
    : device_manager_(device_manager), connection_(connection) {
  DCHECK(connection_);
  DCHECK(device_manager_);
}

WaylandDataDeviceManager::~WaylandDataDeviceManager() = default;

wl_data_device* WaylandDataDeviceManager::GetDevice() {
  DCHECK(connection_->seat());
  return wl_data_device_manager_get_data_device(device_manager_.get(),
                                                connection_->seat());
}

std::unique_ptr<WaylandDataSource> WaylandDataDeviceManager::CreateSource() {
  wl_data_source* data_source =
      wl_data_device_manager_create_data_source(device_manager_.get());
  return std::make_unique<WaylandDataSource>(data_source, connection_);
}

}  // namespace ui
