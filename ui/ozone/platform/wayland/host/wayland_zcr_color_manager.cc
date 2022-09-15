// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zcr_color_manager.h"

#include <chrome-color-management-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
constexpr uint32_t kMaxVersion = 1;
}  // namespace

// static
constexpr char WaylandZcrColorManager::kInterfaceName[];

// static
void WaylandZcrColorManager::Instantiate(WaylandConnection* connection,
                                         wl_registry* registry,
                                         uint32_t name,
                                         const std::string& interface,
                                         uint32_t version) {
  DCHECK_EQ(interface, kInterfaceName);
  if (connection->zcr_color_manager_)
    return;

  auto color_manager = wl::Bind<struct zcr_color_manager_v1>(
      registry, name, std::min(kMinVersion, kMaxVersion));
  if (!color_manager) {
    LOG(ERROR) << "Failed to bind zcr_color_manager_v1";
    return;
  }
  connection->zcr_color_manager_ = std::make_unique<WaylandZcrColorManager>(
      color_manager.release(), connection);
  if (connection->wayland_output_manager())
    connection->wayland_output_manager()->InitializeAllColorManagementOutputs();
}

WaylandZcrColorManager::WaylandZcrColorManager(
    zcr_color_manager_v1* zcr_color_manager,
    WaylandConnection* connection)
    : zcr_color_manager_(zcr_color_manager), connection_(connection) {}

WaylandZcrColorManager::~WaylandZcrColorManager() = default;

wl::Object<zcr_color_management_output_v1>
WaylandZcrColorManager::CreateColorManagementOutput(wl_output* output) {
  return wl::Object<zcr_color_management_output_v1>(
      zcr_color_manager_v1_get_color_management_output(zcr_color_manager_.get(),
                                                       output));
}

}  // namespace ui
