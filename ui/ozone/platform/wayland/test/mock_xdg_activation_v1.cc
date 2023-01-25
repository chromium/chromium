// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_xdg_activation_v1.h"

#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

void SetSerial(struct wl_client* client,
               struct wl_resource* resource,
               uint32_t serial,
               struct wl_resource* seat) {}

void SetAppId(struct wl_client* client,
              struct wl_resource* resource,
              const char* app_id) {}

void SetSurface(struct wl_client* client,
                struct wl_resource* resource,
                struct wl_resource* surface) {
  GetUserDataAs<MockXdgActivationTokenV1>(resource)->SetSurface(
      client, resource, surface);
}

void Commit(struct wl_client* client, struct wl_resource* resource) {
  GetUserDataAs<MockXdgActivationTokenV1>(resource)->Commit(client, resource);
}

}  // namespace

const struct xdg_activation_token_v1_interface kMockXdgActivationTokenV1Impl = {
    &SetSerial,        // set_serial
    &SetAppId,         // set_app_id
    &SetSurface,       // set_surface
    &Commit,           // commit
    &DestroyResource,  // destroy
};

MockXdgActivationTokenV1::MockXdgActivationTokenV1(wl_resource* resource,
                                                   MockXdgActivationV1* global)
    : ServerObject(resource), global_(global) {}

MockXdgActivationTokenV1::~MockXdgActivationTokenV1() = default;

namespace {

void GetActivationToken(struct wl_client* client,
                        struct wl_resource* resource,
                        uint32_t id) {
  auto* global = GetUserDataAs<MockXdgActivationV1>(resource);
  wl_resource* token = CreateResourceWithImpl<MockXdgActivationTokenV1>(
      client, &xdg_activation_token_v1_interface, 1,
      &kMockXdgActivationTokenV1Impl, id, global);
  global->set_token(GetUserDataAs<MockXdgActivationTokenV1>(token));
}

void Activate(struct wl_client* client,
              struct wl_resource* resource,
              const char* token,
              struct wl_resource* surface) {
  GetUserDataAs<MockXdgActivationV1>(resource)->Activate(client, resource,
                                                         token, surface);
}

}  // namespace

const struct xdg_activation_v1_interface kMockXdgActivationV1Impl = {
    &DestroyResource,
    &GetActivationToken,
    &Activate,
};

MockXdgActivationV1::MockXdgActivationV1()
    : GlobalObject(&xdg_activation_v1_interface, &kMockXdgActivationV1Impl, 1) {
}

MockXdgActivationV1::~MockXdgActivationV1() = default;

}  // namespace wl
