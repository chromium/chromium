// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

namespace ui {

class WaylandConnection;
class WaylandDataDevice;

class WaylandDataDeviceManager
    : public wl::GlobalObjectRegistrar<WaylandDataDeviceManager> {
 public:
  static constexpr char kInterfaceName[] = "wl_data_device_manager";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  using DataSource = WaylandDataSource;
  using DataDevice = WaylandDataDevice;

  WaylandDataDeviceManager(wl_data_device_manager* device_manager,
                           WaylandConnection* connection);
  WaylandDataDeviceManager(const WaylandDataDeviceManager&) = delete;
  WaylandDataDeviceManager& operator=(const WaylandDataDeviceManager&) = delete;
  ~WaylandDataDeviceManager();

  WaylandDataDevice* GetDevice();
  std::unique_ptr<WaylandDataSource> CreateSource(
      WaylandDataSource::Delegate* delegate);

 private:
  wl::Object<wl_data_device_manager> device_manager_;

  const raw_ptr<WaylandConnection> connection_;

  std::unique_ptr<WaylandDataDevice> device_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_MANAGER_H_
