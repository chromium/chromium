// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <xdg-shell-unstable-v6-server-protocol.h>

#include "ui/ozone/platform/wayland/test/mock_xdg_popup.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_surface.h"
#include "ui/ozone/platform/wayland/test/test_positioner.h"

namespace wl {

void SetTitle(wl_client* client, wl_resource* resource, const char* title) {
  GetUserDataAs<MockXdgSurface>(resource)->SetTitle(title);
}

void SetAppId(wl_client* client, wl_resource* resource, const char* app_id) {
  GetUserDataAs<MockXdgSurface>(resource)->SetAppId(app_id);
}

void Move(wl_client* client,
          wl_resource* resource,
          wl_resource* seat,
          uint32_t serial) {
  GetUserDataAs<MockXdgSurface>(resource)->Move(serial);
}

void Resize(wl_client* client,
            wl_resource* resource,
            wl_resource* seat,
            uint32_t serial,
            uint32_t edges) {
  GetUserDataAs<MockXdgSurface>(resource)->Resize(serial, edges);
}

void AckConfigure(wl_client* client, wl_resource* resource, uint32_t serial) {
  GetUserDataAs<MockXdgSurface>(resource)->AckConfigure(serial);
}

void SetWindowGeometry(wl_client* client,
                       wl_resource* resource,
                       int32_t x,
                       int32_t y,
                       int32_t width,
                       int32_t height) {
  GetUserDataAs<MockXdgSurface>(resource)->SetWindowGeometry(x, y, width,
                                                             height);
}

void SetMaximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockXdgSurface>(resource)->SetMaximized();
}

void UnsetMaximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockXdgSurface>(resource)->UnsetMaximized();
}

void SetFullscreen(wl_client* client,
                   wl_resource* resource,
                   wl_resource* output) {
  GetUserDataAs<MockXdgSurface>(resource)->SetFullscreen();
}

void UnsetFullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockXdgSurface>(resource)->UnsetFullscreen();
}

void SetMinimized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockXdgSurface>(resource)->SetMinimized();
}

void GetTopLevel(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* surface = GetUserDataAs<MockXdgSurface>(resource);
  if (surface->xdg_toplevel()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }
  wl_resource* xdg_toplevel_resource =
      wl_resource_create(client, &zxdg_toplevel_v6_interface, 1, id);
  if (!xdg_toplevel_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  surface->set_xdg_toplevel(
      std::make_unique<MockXdgTopLevel>(xdg_toplevel_resource));
}

void GetZXdgPopupV6(struct wl_client* client,
                    struct wl_resource* resource,
                    uint32_t id,
                    struct wl_resource* parent,
                    struct wl_resource* positioner_resource) {
  auto* mock_xdg_surface = GetUserDataAs<MockXdgSurface>(resource);
  wl_resource* current_resource = mock_xdg_surface->resource();
  if (current_resource &&
      (ResourceHasImplementation(current_resource, &zxdg_popup_v6_interface,
                                 &kZxdgPopupV6Impl) ||
       ResourceHasImplementation(current_resource,
                                 &zxdg_positioner_v6_interface,
                                 &kTestZxdgPositionerV6Impl))) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  wl_resource* xdg_popup_resource = wl_resource_create(
      client, &zxdg_popup_v6_interface, wl_resource_get_version(resource), id);
  auto* positioner = GetUserDataAs<TestPositioner>(positioner_resource);
  DCHECK(positioner);

  auto mock_xdg_popup =
      std::make_unique<MockXdgPopup>(xdg_popup_resource, &kZxdgPopupV6Impl);
  mock_xdg_popup->set_position(positioner->position());
  if (mock_xdg_popup->size().IsEmpty() ||
      mock_xdg_popup->anchor_rect().IsEmpty()) {
    wl_resource_post_error(resource, ZXDG_SHELL_V6_ERROR_INVALID_POSITIONER,
                           "Positioner object is not complete");
    return;
  }

  mock_xdg_surface->set_xdg_popup(std::move(mock_xdg_popup));
}

const struct xdg_surface_interface kMockXdgSurfaceImpl = {
    &DestroyResource,    // destroy
    nullptr,             // set_parent
    &SetTitle,           // set_title
    &SetAppId,           // set_app_id
    nullptr,             // show_window_menu
    &Move,               // move
    &Resize,             // resize
    &AckConfigure,       // ack_configure
    &SetWindowGeometry,  // set_window_geometry
    &SetMaximized,       // set_maximized
    &UnsetMaximized,     // set_unmaximized
    &SetFullscreen,      // set_fullscreen
    &UnsetFullscreen,    // unset_fullscreen
    &SetMinimized,       // set_minimized
};

const struct zxdg_surface_v6_interface kMockZxdgSurfaceV6Impl = {
    &DestroyResource,    // destroy
    &GetTopLevel,        // get_toplevel
    &GetZXdgPopupV6,     // get_popup
    &SetWindowGeometry,  // set_window_geometry
    &AckConfigure,       // ack_configure
};

const struct zxdg_toplevel_v6_interface kMockZxdgToplevelV6Impl = {
    &DestroyResource,  // destroy
    nullptr,           // set_parent
    &SetTitle,         // set_title
    &SetAppId,         // set_app_id
    nullptr,           // show_window_menu
    &Move,             // move
    &Resize,           // resize
    nullptr,           // set_max_size
    nullptr,           // set_min_size
    &SetMaximized,     // set_maximized
    &UnsetMaximized,   // set_unmaximized
    &SetFullscreen,    // set_fullscreen
    &UnsetFullscreen,  // unset_fullscreen
    &SetMinimized,     // set_minimized
};

MockXdgSurface::MockXdgSurface(wl_resource* resource,
                               const void* implementation)
    : ServerObject(resource) {
  SetImplementationUnretained(resource, implementation, this);
}

MockXdgSurface::~MockXdgSurface() {}

MockXdgTopLevel::MockXdgTopLevel(wl_resource* resource)
    : MockXdgSurface(resource, &kMockZxdgSurfaceV6Impl) {
  SetImplementationUnretained(resource, &kMockZxdgToplevelV6Impl, this);
}

MockXdgTopLevel::~MockXdgTopLevel() {}

}  // namespace wl
