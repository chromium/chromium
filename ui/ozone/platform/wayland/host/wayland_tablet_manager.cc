// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_tablet_manager.h"

#include <tablet-unstable-v2-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kMaxVersion = 1;
}

// static
constexpr char WaylandTabletManager::kInterfaceName[];

// static
void WaylandTabletManager::Instantiate(WaylandConnection* connection,
                                       wl_registry* registry,
                                       uint32_t name,
                                       const std::string& interface,
                                       uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->tablet_manager_) {
    return;
  }

  auto manager = wl::Bind<zwp_tablet_manager_v2>(
      registry, name, std::min(version, kMaxVersion));
  if (!manager) {
    LOG(ERROR) << "Failed to bind " << kInterfaceName;
    return;
  }
  connection->tablet_manager_ =
      std::make_unique<WaylandTabletManager>(manager.release(), connection);
}

WaylandTabletManager::WaylandTabletManager(zwp_tablet_manager_v2* manager,
                                           WaylandConnection* connection)
    : manager_(manager), connection_(connection) {}

WaylandTabletManager::~WaylandTabletManager() = default;

wl::Object<zwp_tablet_seat_v2> WaylandTabletManager::GetTabletSeat(
    wl_seat* seat) {
  return wl::Object<zwp_tablet_seat_v2>(
      zwp_tablet_manager_v2_get_tablet_seat(manager_.get(), seat));
}

}  // namespace ui
