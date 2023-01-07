// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_DEVICE_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_DEVICE_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

namespace ui {

class GtkPrimarySelectionDevice;
class WaylandConnection;

class GtkPrimarySelectionDeviceManager
    : public wl::GlobalObjectRegistrar<GtkPrimarySelectionDeviceManager> {
 public:
  using DataSource = GtkPrimarySelectionSource;
  using DataDevice = GtkPrimarySelectionDevice;

  static constexpr char kInterfaceName[] =
      "gtk_primary_selection_device_manager";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  GtkPrimarySelectionDeviceManager(
      gtk_primary_selection_device_manager* manager,
      WaylandConnection* connection);
  GtkPrimarySelectionDeviceManager(const GtkPrimarySelectionDeviceManager&) =
      delete;
  GtkPrimarySelectionDeviceManager& operator=(
      const GtkPrimarySelectionDeviceManager&) = delete;
  ~GtkPrimarySelectionDeviceManager();

  GtkPrimarySelectionDevice* GetDevice();
  std::unique_ptr<GtkPrimarySelectionSource> CreateSource(
      GtkPrimarySelectionSource::Delegate* delegate);

 private:
  wl::Object<gtk_primary_selection_device_manager> device_manager_;

  const raw_ptr<WaylandConnection> connection_;

  std::unique_ptr<GtkPrimarySelectionDevice> device_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_DEVICE_MANAGER_H_
