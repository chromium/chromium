// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device_manager.h"

#include <gtk-primary-selection-client-protocol.h>

#include "ui/ozone/platform/wayland/host/gtk_primary_selection_source.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

GtkPrimarySelectionDeviceManager::GtkPrimarySelectionDeviceManager(
    gtk_primary_selection_device_manager* manager,
    WaylandConnection* connection)
    : gtk_primary_selection_device_manager_(manager), connection_(connection) {
  DCHECK(connection_);
  DCHECK(gtk_primary_selection_device_manager_);
}

GtkPrimarySelectionDeviceManager::~GtkPrimarySelectionDeviceManager() = default;

gtk_primary_selection_device* GtkPrimarySelectionDeviceManager::GetDevice() {
  DCHECK(connection_->seat());
  return gtk_primary_selection_device_manager_get_device(
      gtk_primary_selection_device_manager_.get(), connection_->seat());
}

std::unique_ptr<GtkPrimarySelectionSource>
GtkPrimarySelectionDeviceManager::CreateSource() {
  gtk_primary_selection_source* data_source =
      gtk_primary_selection_device_manager_create_source(
          gtk_primary_selection_device_manager_.get());
  return std::make_unique<GtkPrimarySelectionSource>(data_source, connection_);
}

}  // namespace ui
