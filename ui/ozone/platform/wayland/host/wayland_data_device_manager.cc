// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"

#include <memory>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

namespace ui {

namespace {
constexpr uint32_t kMaxDeviceManagerVersion = 3;
}

// static
void WaylandDataDeviceManager::Register(WaylandConnection* connection) {
  connection->RegisterGlobalObjectFactory(
      "wl_data_device_manager", &WaylandDataDeviceManager::Instantiate);
}

// static
void WaylandDataDeviceManager::Instantiate(WaylandConnection* connection,
                                           wl_registry* registry,
                                           uint32_t name,
                                           uint32_t version) {
  if (connection->data_device_manager_)
    return;

  auto data_device_manager = wl::Bind<wl_data_device_manager>(
      registry, name, std::min(version, kMaxDeviceManagerVersion));
  if (!data_device_manager) {
    LOG(ERROR) << "Failed to bind to wl_data_device_manager global";
    return;
  }
  connection->data_device_manager_ = std::make_unique<WaylandDataDeviceManager>(
      data_device_manager.release(), connection);
  connection->CreateDataObjectsIfReady();
}

WaylandDataDeviceManager::WaylandDataDeviceManager(
    wl_data_device_manager* device_manager,
    WaylandConnection* connection)
    : device_manager_(device_manager), connection_(connection) {
  DCHECK(connection_);
  DCHECK(device_manager_);
}

WaylandDataDeviceManager::~WaylandDataDeviceManager() = default;

WaylandDataDevice* WaylandDataDeviceManager::GetDevice() {
  DCHECK(connection_->seat());
  if (!device_) {
    device_ = std::make_unique<WaylandDataDevice>(
        connection_, wl_data_device_manager_get_data_device(
                         device_manager_.get(), connection_->seat()));
  }
  DCHECK(device_);
  return device_.get();
}

std::unique_ptr<WaylandDataSource> WaylandDataDeviceManager::CreateSource(
    WaylandDataSource::Delegate* delegate) {
  wl_data_source* data_source =
      wl_data_device_manager_create_data_source(device_manager_.get());
  return std::make_unique<WaylandDataSource>(data_source, connection_,
                                             delegate);
}

}  // namespace ui
