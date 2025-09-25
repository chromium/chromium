// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_idle_inhibit_manager.h"

#include <idle-inhibit-unstable-v1-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

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
    : connection_(connection), manager_(manager) {}

ZwpIdleInhibitManager::~ZwpIdleInhibitManager() = default;

bool ZwpIdleInhibitManager::CreateInhibitor() {
  // Wayland inhibits idle behaviour on certain output, and implies that a
  // surface bound to that output should obtain the inhibitor and hold it
  // until it no longer needs to prevent the output to go idle.
  // We assume that the idle lock is initiated by the user, and therefore the
  // surface that we should use is the one owned by the window that is focused
  // currently.
  const auto* window_manager = connection_->window_manager();
  DCHECK(window_manager);
  auto* current_window = window_manager->GetCurrentFocusedWindow();
  if (!current_window) {
    LOG(WARNING) << "Cannot inhibit going idle when no window is focused";
    return false;
  }

  DCHECK(current_window->root_surface());
  auto new_inhibitor = wl::Object<zwp_idle_inhibitor_v1>(
      zwp_idle_inhibit_manager_v1_create_inhibitor(
          manager_.get(), current_window->root_surface()->surface()));

  idle_inhibitor_.swap(new_inhibitor);
  return true;
}

void ZwpIdleInhibitManager::RemoveInhibitor() {
  inhibiting_window_ = nullptr;
  idle_inhibitor_.reset();
}

}  // namespace ui
