// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_idle_inhibit_manager.h"

#include <idle-inhibit-unstable-v1-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
}

// static
constexpr char ZwpIdleInhibitManager::kInterfaceName[];

// static
void ZwpIdleInhibitManager::Instantiate(WaylandConnection* connection,
                                        wl_registry* registry,
                                        uint32_t name,
                                        const std::string& interface,
                                        uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->zwp_idle_inhibit_manager_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto manager =
      wl::Bind<zwp_idle_inhibit_manager_v1>(registry, name, kMinVersion);
  if (!manager) {
    LOG(ERROR) << "Failed to bind zwp_idle_inhibit_manager_v1";
    return;
  }
  connection->zwp_idle_inhibit_manager_ =
      std::make_unique<ZwpIdleInhibitManager>(manager.release(), connection);
}

ZwpIdleInhibitManager::ZwpIdleInhibitManager(
    zwp_idle_inhibit_manager_v1* manager,
    WaylandConnection* connection)
    : manager_(manager) {}

ZwpIdleInhibitManager::~ZwpIdleInhibitManager() = default;

wl::Object<zwp_idle_inhibitor_v1> ZwpIdleInhibitManager::CreateInhibitor(
    wl_surface* surface) {
  return wl::Object<zwp_idle_inhibitor_v1>(
      zwp_idle_inhibit_manager_v1_create_inhibitor(manager_.get(), surface));
}

}  // namespace ui
