// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_primary_selection_device_manager.h"

#include <primary-selection-unstable-v1-client-protocol.h>

#include <memory>

#include "ui/ozone/platform/wayland/host/zwp_primary_selection_device.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

namespace ui {

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
        connection_, zwp_primary_selection_device_manager_v1_get_device(
                         device_manager_.get(), connection_->seat()));
  }
  DCHECK(device_);
  return device_.get();
}

std::unique_ptr<ZwpPrimarySelectionSource>
ZwpPrimarySelectionDeviceManager::CreateSource(
    ZwpPrimarySelectionSource::Delegate* delegate) {
  auto* data_source = zwp_primary_selection_device_manager_v1_create_source(
      device_manager_.get());
  return std::make_unique<ZwpPrimarySelectionSource>(data_source, connection_,
                                                  delegate);
}

}  // namespace ui
