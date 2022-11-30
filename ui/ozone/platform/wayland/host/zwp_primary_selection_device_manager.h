// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_PRIMARY_SELECTION_DEVICE_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_PRIMARY_SELECTION_DEVICE_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

namespace ui {

class ZwpPrimarySelectionDevice;
class WaylandConnection;

class ZwpPrimarySelectionDeviceManager
    : public wl::GlobalObjectRegistrar<ZwpPrimarySelectionDeviceManager> {
 public:
  static constexpr char kInterfaceName[] =
      "zwp_primary_selection_device_manager_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  using DataSource = ZwpPrimarySelectionSource;
  using DataDevice = ZwpPrimarySelectionDevice;

  ZwpPrimarySelectionDeviceManager(
      zwp_primary_selection_device_manager_v1* manager,
      WaylandConnection* connection);
  ZwpPrimarySelectionDeviceManager(const ZwpPrimarySelectionDeviceManager&) =
      delete;
  ZwpPrimarySelectionDeviceManager& operator=(
      const ZwpPrimarySelectionDeviceManager&) = delete;
  ~ZwpPrimarySelectionDeviceManager();

  ZwpPrimarySelectionDevice* GetDevice();
  std::unique_ptr<ZwpPrimarySelectionSource> CreateSource(
      ZwpPrimarySelectionSource::Delegate* delegate);

 private:
  wl::Object<zwp_primary_selection_device_manager_v1> device_manager_;

  const raw_ptr<WaylandConnection> connection_;

  std::unique_ptr<ZwpPrimarySelectionDevice> device_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_PRIMARY_SELECTION_DEVICE_MANAGER_H_
