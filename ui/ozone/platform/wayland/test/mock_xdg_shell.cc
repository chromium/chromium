// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_xdg_shell.h"

#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_popup.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_positioner.h"

namespace wl {

namespace {

constexpr uint32_t kXdgShellVersion = 1;

void GetXdgSurfaceImpl(wl_client* client,
                       wl_resource* resource,
                       uint32_t id,
                       wl_resource* surface_resource,
                       const struct wl_interface* interface,
                       const void* implementation) {
  auto* surface = GetUserDataAs<MockSurface>(surface_resource);
  if (surface->xdg_surface()) {
    uint32_t xdg_error = implementation == &kMockXdgSurfaceImpl
                             ? XDG_SHELL_ERROR_ROLE
                             : ZXDG_SHELL_V6_ERROR_ROLE;
    wl_resource_post_error(resource, xdg_error, "surface already has a role");
    return;
  }
  wl_resource* xdg_surface_resource = wl_resource_create(
      client, interface, wl_resource_get_version(resource), id);

  if (!xdg_surface_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  surface->set_xdg_surface(
      std::make_unique<MockXdgSurface>(xdg_surface_resource, implementation));
}

void UseUnstableVersion(wl_client* client,
                        wl_resource* resource,
                        int32_t version) {
  GetUserDataAs<MockXdgShell>(resource)->UseUnstableVersion(version);
}

void GetXdgSurface(wl_client* client,
                   wl_resource* resource,
                   uint32_t id,
                   wl_resource* surface_resource) {
  GetXdgSurfaceImpl(client, resource, id, surface_resource,
                    &xdg_surface_interface, &kMockXdgSurfaceImpl);
}

void GetXdgPopup(struct wl_client* client,
                 struct wl_resource* resource,
                 uint32_t id,
                 struct wl_resource* surface,
                 struct wl_resource* parent,
                 struct wl_resource* seat,
                 uint32_t serial,
                 int32_t x,
                 int32_t y) {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface);
  if (mock_surface->resource() &&
      ResourceHasImplementation(mock_surface->resource(), &xdg_popup_interface,
                                &kXdgPopupImpl)) {
    wl_resource_post_error(resource, XDG_SHELL_ERROR_ROLE,
                           "surface has already assigned a role");
    return;
  }

  wl_resource* xdg_popup_resource = wl_resource_create(
      client, &xdg_popup_interface, wl_resource_get_version(resource), id);

  auto mock_xdg_popup =
      std::make_unique<MockXdgPopup>(xdg_popup_resource, &kXdgPopupImpl);

  mock_surface->set_xdg_popup(std::move(mock_xdg_popup));
}

void Pong(wl_client* client, wl_resource* resource, uint32_t serial) {
  GetUserDataAs<MockXdgShell>(resource)->Pong(serial);
}

void CreatePositioner(wl_client* client,
                      struct wl_resource* resource,
                      uint32_t id) {
  CreateResourceWithImpl<TestPositioner>(client, &zxdg_positioner_v6_interface,
                                         wl_resource_get_version(resource),
                                         &kTestZxdgPositionerV6Impl, id);
}

void GetXdgSurfaceV6(wl_client* client,
                     wl_resource* resource,
                     uint32_t id,
                     wl_resource* surface_resource) {
  GetXdgSurfaceImpl(client, resource, id, surface_resource,
                    &zxdg_surface_v6_interface, &kMockZxdgSurfaceV6Impl);
}

void PongV6(wl_client* client, wl_resource* resource, uint32_t serial) {
  GetUserDataAs<MockZxdgShellV6>(resource)->Pong(serial);
}

}  // namespace

const struct xdg_shell_interface kMockXdgShellImpl = {
    &DestroyResource,     // destroy
    &UseUnstableVersion,  // use_unstable_version
    &GetXdgSurface,       // get_xdg_surface
    &GetXdgPopup,         // get_xdg_popup
    &Pong,                // pong
};

const struct zxdg_shell_v6_interface kMockZxdgShellV6Impl = {
    &DestroyResource,   // destroy
    &CreatePositioner,  // create_positioner
    &GetXdgSurfaceV6,   // get_xdg_surface
    &PongV6,            // pong
};

MockXdgShell::MockXdgShell()
    : GlobalObject(&xdg_shell_interface, &kMockXdgShellImpl, kXdgShellVersion) {
}

MockXdgShell::~MockXdgShell() {}

MockZxdgShellV6::MockZxdgShellV6()
    : GlobalObject(&zxdg_shell_v6_interface,
                   &kMockZxdgShellV6Impl,
                   kXdgShellVersion) {}

MockZxdgShellV6::~MockZxdgShellV6() {}

}  // namespace wl
