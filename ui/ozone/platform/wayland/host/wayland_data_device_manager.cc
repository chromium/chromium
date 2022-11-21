// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"

#include <memory>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
constexpr uint32_t kMaxVersion = 3;
}

// static
constexpr char WaylandDataDeviceManager::kInterfaceName[];

// static
void WaylandDataDeviceManager::Instantiate(WaylandConnection* connection,
                                           wl_registry* registry,
                                           uint32_t name,
                                           const std::string& interface,
                                           uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->data_device_manager_ ||
      !wl::CanBind(interface, version, kMinVersion, kMaxVersion)) {
    return;
  }

  auto data_device_manager = wl::Bind<wl_data_device_manager>(
      registry, name, std::min(version, kMaxVersion));
  if (!data_device_manager) {
    LOG(ERROR) << "Failed to bind to wl_data_device_manager global";
    return;
  }
  connection->data_device_manager_ = std::make_unique<WaylandDataDeviceManager>(
      data_device_manager.release(), connection);

  // The data device manager is one of objects needed for data exchange.  Notify
  // the connection so it might set up the rest if all other parts are in place.
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
        connection_,
        wl_data_device_manager_get_data_device(
            device_manager_.get(), connection_->seat()->wl_object()));
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
