// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zxdg_decoration_manager.h"

#include <xdg-decoration-unstable-v1-server-protocol.h>
#include <xdg-shell-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_surface.h"
#include "ui/ozone/platform/wayland/test/scoped_wl_array.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

constexpr uint32_t kTestZxdgDecorationManagerV1Version = 1;

// State of a zxdg_toplevel_decoration_v1, owned by its resource.
struct ToplevelDecoration {
  // The manager global outlives every decoration resource created from it.
  raw_ptr<TestZxdgDecorationManagerV1> manager;
  base::WeakPtr<MockXdgSurface> xdg_surface;
};

void DestroyToplevelDecoration(wl_resource* resource) {
  delete GetUserDataAs<ToplevelDecoration>(resource);
}

void SetMode(wl_client* client, wl_resource* resource, uint32_t mode) {
  auto* decoration = GetUserDataAs<ToplevelDecoration>(resource);
  TestZxdgDecorationManagerV1* manager = decoration->manager;
  manager->requested_modes().push_back(mode);
  zxdg_toplevel_decoration_v1_send_configure(
      resource, manager->forced_mode() ? manager->forced_mode() : mode);

  // The decoration mode is applied with the xdg_surface.configure that
  // follows.
  const std::optional<gfx::Size>& size = manager->toplevel_configure_size();
  if (!size) {
    return;
  }
  MockXdgSurface* xdg_surface = decoration->xdg_surface.get();
  if (!xdg_surface || !xdg_surface->xdg_toplevel()) {
    return;
  }
  ScopedWlArray states({});
  xdg_toplevel_send_configure(xdg_surface->xdg_toplevel()->resource(),
                              size->width(), size->height(), states.get());
  xdg_surface_send_configure(
      xdg_surface->resource(),
      wl_display_next_serial(wl_client_get_display(client)));
}

void UnsetMode(wl_client* client, wl_resource* resource) {}

constexpr struct zxdg_toplevel_decoration_v1_interface kToplevelDecorationImpl =
    {
        .destroy = DestroyResource,
        .set_mode = SetMode,
        .unset_mode = UnsetMode,
};

void GetToplevelDecoration(wl_client* client,
                           wl_resource* resource,
                           uint32_t id,
                           wl_resource* toplevel) {
  wl_resource* decoration =
      wl_resource_create(client, &zxdg_toplevel_decoration_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!decoration) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(
      decoration, &kToplevelDecorationImpl,
      new ToplevelDecoration{
          GetUserDataAs<TestZxdgDecorationManagerV1>(resource),
          GetUserDataAs<MockXdgTopLevel>(toplevel)
              ->xdg_surface()
              ->GetWeakPtr()},
      &DestroyToplevelDecoration);
}

constexpr struct zxdg_decoration_manager_v1_interface kDecorationManagerImpl = {
    .destroy = DestroyResource,
    .get_toplevel_decoration = GetToplevelDecoration,
};

}  // namespace

TestZxdgDecorationManagerV1::TestZxdgDecorationManagerV1()
    : GlobalObject(&zxdg_decoration_manager_v1_interface,
                   &kDecorationManagerImpl,
                   kTestZxdgDecorationManagerV1Version) {}

TestZxdgDecorationManagerV1::~TestZxdgDecorationManagerV1() = default;

}  // namespace wl
