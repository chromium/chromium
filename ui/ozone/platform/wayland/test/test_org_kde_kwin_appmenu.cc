// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_org_kde_kwin_appmenu.h"

#include <appmenu-server-protocol.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

constexpr uint32_t kTestOrgKdeKwinAppmenuManagerVersion = 1;

void SetAddress(wl_client* client,
                wl_resource* resource,
                const char* service_name,
                const char* object_path) {}

constexpr struct org_kde_kwin_appmenu_interface kAppmenuImpl = {
    .set_address = SetAddress,
    .release = DestroyResource,
};

void Create(wl_client* client,
            wl_resource* resource,
            uint32_t id,
            wl_resource* surface) {
  wl_resource* appmenu =
      wl_resource_create(client, &org_kde_kwin_appmenu_interface,
                         wl_resource_get_version(resource), id);
  if (!appmenu) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(appmenu, &kAppmenuImpl, nullptr, nullptr);
}

constexpr struct org_kde_kwin_appmenu_manager_interface kAppmenuManagerImpl = {
    .create = Create,
};

}  // namespace

TestOrgKdeKwinAppmenuManager::TestOrgKdeKwinAppmenuManager()
    : GlobalObject(&org_kde_kwin_appmenu_manager_interface,
                   &kAppmenuManagerImpl,
                   kTestOrgKdeKwinAppmenuManagerVersion) {}

TestOrgKdeKwinAppmenuManager::~TestOrgKdeKwinAppmenuManager() = default;

}  // namespace wl
