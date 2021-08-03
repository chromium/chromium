// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_idle_inhibit_manager.h"

#include <idle-inhibit-unstable-v1-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kMaxZwpIdleInhibitManagerVersion = 1;
}

// static
void ZwpIdleInhibitManager::Register(WaylandConnection* connection) {
  connection->RegisterGlobalObjectFactory("zwp_idle_inhibit_manager_v1",
                                          &ZwpIdleInhibitManager::Instantiate);
}

// static
void ZwpIdleInhibitManager::Instantiate(WaylandConnection* connection,
                                        wl_registry* registry,
                                        uint32_t name,
                                        uint32_t version) {
  if (connection->zwp_idle_inhibit_manager_)
    return;

  auto manager = wl::Bind<zwp_idle_inhibit_manager_v1>(
      registry, name, std::min(version, kMaxZwpIdleInhibitManagerVersion));
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
