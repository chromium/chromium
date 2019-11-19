// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_DEVICE_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_DEVICE_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

struct gtk_primary_selection_device_manager;
struct gtk_primary_selection_device;

namespace ui {

class GtkPrimarySelectionSource;
class WaylandConnection;

class GtkPrimarySelectionDeviceManager {
 public:
  GtkPrimarySelectionDeviceManager(
      gtk_primary_selection_device_manager* manager,
      WaylandConnection* connection);
  ~GtkPrimarySelectionDeviceManager();

  gtk_primary_selection_device* GetDevice();
  std::unique_ptr<GtkPrimarySelectionSource> CreateSource();

 private:
  wl::Object<gtk_primary_selection_device_manager>
      gtk_primary_selection_device_manager_;

  WaylandConnection* connection_;

  DISALLOW_COPY_AND_ASSIGN(GtkPrimarySelectionDeviceManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_DEVICE_MANAGER_H_
