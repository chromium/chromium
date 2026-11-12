// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_xx_session_manager.h"

#include <xx-session-management-v1-server-protocol.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

constexpr uint32_t kTestXxSessionManagerVersion = 1;

constexpr struct xx_toplevel_session_v1_interface kToplevelSessionImpl = {
    .destroy = DestroyResource,
    .remove = DestroyResource,
};

void CreateToplevelSession(wl_client* client,
                           wl_resource* resource,
                           uint32_t id) {
  wl_resource* toplevel_session =
      wl_resource_create(client, &xx_toplevel_session_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!toplevel_session) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(toplevel_session, &kToplevelSessionImpl,
                                 nullptr, nullptr);
}

void AddToplevel(wl_client* client,
                 wl_resource* resource,
                 uint32_t id,
                 wl_resource* toplevel,
                 const char* name) {
  CreateToplevelSession(client, resource, id);
}

void RestoreToplevel(wl_client* client,
                     wl_resource* resource,
                     uint32_t id,
                     wl_resource* toplevel,
                     const char* name) {
  CreateToplevelSession(client, resource, id);
}

constexpr struct xx_session_v1_interface kSessionImpl = {
    .destroy = DestroyResource,
    .remove = DestroyResource,
    .add_toplevel = AddToplevel,
    .restore_toplevel = RestoreToplevel,
};

void GetSession(wl_client* client,
                wl_resource* resource,
                uint32_t id,
                uint32_t reason,
                const char* session_id) {
  wl_resource* session = wl_resource_create(
      client, &xx_session_v1_interface, wl_resource_get_version(resource), id);
  if (!session) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(session, &kSessionImpl, nullptr, nullptr);
  if (session_id) {
    xx_session_v1_send_restored(session);
  } else {
    xx_session_v1_send_created(session, TestXxSessionManager::kTestSessionId);
  }
}

constexpr struct xx_session_manager_v1_interface kSessionManagerImpl = {
    .destroy = DestroyResource,
    .get_session = GetSession,
};

}  // namespace

TestXxSessionManager::TestXxSessionManager()
    : GlobalObject(&xx_session_manager_v1_interface,
                   &kSessionManagerImpl,
                   kTestXxSessionManagerVersion) {}

TestXxSessionManager::~TestXxSessionManager() = default;

}  // namespace wl
