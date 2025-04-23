// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/org_kde_kwin_appmenu.h"

#include <appmenu-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kVersion = 1;
}  // namespace

// static
constexpr char OrgKdeKwinAppmenuManager::kInterfaceName[];

// static
void OrgKdeKwinAppmenuManager::Instantiate(WaylandConnection* connection,
                                           wl_registry* registry,
                                           uint32_t name,
                                           const std::string& interface,
                                           uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->org_kde_kwin_appmenu_manager_ ||
      !wl::CanBind(interface, version, kVersion, kVersion)) {
    return;
  }

  auto manager =
      wl::Bind<org_kde_kwin_appmenu_manager>(registry, name, kVersion);
  if (!manager) {
    LOG(ERROR) << "Failed to bind to org_kde_kwin_appmenu_manager global";
    return;
  }
  connection->org_kde_kwin_appmenu_manager_ =
      std::make_unique<OrgKdeKwinAppmenuManager>(manager.release(), connection);
}

OrgKdeKwinAppmenuManager::OrgKdeKwinAppmenuManager(
    org_kde_kwin_appmenu_manager* manager,
    WaylandConnection* connection)
    : manager_(manager), connection_(connection) {}

OrgKdeKwinAppmenuManager::~OrgKdeKwinAppmenuManager() = default;

std::unique_ptr<OrgKdeKwinAppmenu> OrgKdeKwinAppmenuManager::Create(
    wl_surface* surface) {
  return std::make_unique<OrgKdeKwinAppmenu>(
      org_kde_kwin_appmenu_manager_create(manager_.get(), surface));
}

OrgKdeKwinAppmenu::OrgKdeKwinAppmenu(org_kde_kwin_appmenu* appmenu)
    : appmenu_(appmenu) {}

OrgKdeKwinAppmenu::~OrgKdeKwinAppmenu() = default;

void OrgKdeKwinAppmenu::SetAddress(const std::string& service_name,
                                   const std::string& object_path) {
  org_kde_kwin_appmenu_set_address(appmenu_.get(), service_name.c_str(),
                                   object_path.c_str());
}

}  // namespace ui
