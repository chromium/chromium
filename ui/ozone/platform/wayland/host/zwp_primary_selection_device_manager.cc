// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_primary_selection_device_manager.h"

#include <primary-selection-unstable-v1-client-protocol.h>

#include <memory>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/zwp_primary_selection_device.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
}  // namespace

// static
constexpr char ZwpPrimarySelectionDeviceManager::kInterfaceName[];

// static
void ZwpPrimarySelectionDeviceManager::Instantiate(
    WaylandConnection* connection,
    wl_registry* registry,
    uint32_t name,
    const std::string& interface,
    uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->zwp_primary_selection_device_manager_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto manager = wl::Bind<zwp_primary_selection_device_manager_v1>(
      registry, name, kMinVersion);
  if (!manager) {
    LOG(ERROR) << "Failed to bind zwp_primary_selection_device_manager_v1";
    return;
  }
  connection->zwp_primary_selection_device_manager_ =
      std::make_unique<ZwpPrimarySelectionDeviceManager>(manager.release(),
                                                         connection);
}

ZwpPrimarySelectionDeviceManager::ZwpPrimarySelectionDeviceManager(
    zwp_primary_selection_device_manager_v1* manager,
    WaylandConnection* connection)
    : device_manager_(manager), connection_(connection) {
  DCHECK(connection_);
  DCHECK(device_manager_);
}

ZwpPrimarySelectionDeviceManager::~ZwpPrimarySelectionDeviceManager() = default;

ZwpPrimarySelectionDevice* ZwpPrimarySelectionDeviceManager::GetDevice() {
  DCHECK(connection_->seat());
  if (!device_) {
    device_ = std::make_unique<ZwpPrimarySelectionDevice>(
        connection_,
        zwp_primary_selection_device_manager_v1_get_device(
            device_manager_.get(), connection_->seat()->wl_object()));
    connection_->Flush();
  }
  DCHECK(device_);
  return device_.get();
}

std::unique_ptr<ZwpPrimarySelectionSource>
ZwpPrimarySelectionDeviceManager::CreateSource(
    ZwpPrimarySelectionSource::Delegate* delegate) {
  auto* data_source = zwp_primary_selection_device_manager_v1_create_source(
      device_manager_.get());
  connection_->Flush();
  return std::make_unique<ZwpPrimarySelectionSource>(data_source, connection_,
                                                     delegate);
}

}  // namespace ui
