// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_xdg_shell.h"

#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_positioner.h"
#include "ui/ozone/platform/wayland/test/test_xdg_popup.h"

namespace wl {

namespace {

constexpr uint32_t kXdgShellVersion = 3;
constexpr uint32_t kZXdgShellVersion = 1;

void GetXdgSurfaceImpl(wl_client* client,
                       wl_resource* resource,
                       uint32_t id,
                       wl_resource* surface_resource,
                       const struct wl_interface* interface,
                       const void* implementation) {
  auto* surface = GetUserDataAs<MockSurface>(surface_resource);
  if (surface->xdg_surface()) {
    uint32_t xdg_error = implementation == &kMockXdgSurfaceImpl
                             ? static_cast<uint32_t>(XDG_WM_BASE_ERROR_ROLE)
                             : static_cast<uint32_t>(ZXDG_SHELL_V6_ERROR_ROLE);
    wl_resource_post_error(resource, xdg_error, "surface already has a role");
    return;
  }

  wl_resource* xdg_surface_resource =
      CreateResourceWithImpl<::testing::NiceMock<MockXdgSurface>>(
          client, interface, wl_resource_get_version(resource), implementation,
          id, surface_resource);
  if (!xdg_surface_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  surface->set_xdg_surface(GetUserDataAs<MockXdgSurface>(xdg_surface_resource));
}

void CreatePositioner(wl_client* client,
                      struct wl_resource* resource,
                      uint32_t id) {
  CreateResourceWithImpl<TestPositioner>(client, &xdg_positioner_interface,
                                         wl_resource_get_version(resource),
                                         &kTestXdgPositionerImpl, id);
}

void GetXdgSurface(wl_client* client,
                   wl_resource* resource,
                   uint32_t id,
                   wl_resource* surface_resource) {
  GetXdgSurfaceImpl(client, resource, id, surface_resource,
                    &xdg_surface_interface, &kMockXdgSurfaceImpl);
}

void Pong(wl_client* client, wl_resource* resource, uint32_t serial) {
  GetUserDataAs<MockXdgShell>(resource)->Pong(serial);
}

void CreatePositionerV6(wl_client* client,
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

const struct xdg_wm_base_interface kMockXdgShellImpl = {
    &DestroyResource,   // destroy
    &CreatePositioner,  // create_positioner
    &GetXdgSurface,     // get_xdg_surface
    &Pong,              // pong
};

const struct zxdg_shell_v6_interface kMockZxdgShellV6Impl = {
    &DestroyResource,     // destroy
    &CreatePositionerV6,  // create_positioner
    &GetXdgSurfaceV6,     // get_xdg_surface
    &PongV6,              // pong
};

MockXdgShell::MockXdgShell()
    : GlobalObject(&xdg_wm_base_interface,
                   &kMockXdgShellImpl,
                   kXdgShellVersion) {}

MockXdgShell::~MockXdgShell() {}

MockZxdgShellV6::MockZxdgShellV6()
    : GlobalObject(&zxdg_shell_v6_interface,
                   &kMockZxdgShellV6Impl,
                   kZXdgShellVersion) {}

MockZxdgShellV6::~MockZxdgShellV6() {}

}  // namespace wl
