// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_surface.h"
#include "ui/ozone/platform/wayland/test/test_positioner.h"
#include "ui/ozone/platform/wayland/test/test_xdg_popup.h"

namespace wl {

void SetTitle(wl_client* client, wl_resource* resource, const char* title) {
  auto* toplevel = GetUserDataAs<MockXdgTopLevel>(resource);
  // As it this can be envoked during construction of the XdgSurface, cache the
  // result so that tests are able to access that information.
  toplevel->set_title(title);
  toplevel->SetTitle(toplevel->title());
}

void SetAppId(wl_client* client, wl_resource* resource, const char* app_id) {
  auto* toplevel = GetUserDataAs<MockXdgTopLevel>(resource);
  toplevel->SetAppId(app_id);
  // As it this can be envoked during construction of the XdgSurface, cache the
  // result so that tests are able to access that information.
  toplevel->set_app_id(app_id);
}

void Move(wl_client* client,
          wl_resource* resource,
          wl_resource* seat,
          uint32_t serial) {
  GetUserDataAs<MockXdgTopLevel>(resource)->Move(serial);
}

void Resize(wl_client* client,
            wl_resource* resource,
            wl_resource* seat,
            uint32_t serial,
            uint32_t edges) {
  GetUserDataAs<MockXdgTopLevel>(resource)->Resize(serial, edges);
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
  GetUserDataAs<MockXdgSurface>(resource)->SetWindowGeometry(
      {x, y, width, height});
}

void SetMaximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockXdgTopLevel>(resource)->SetMaximized();
}

void UnsetMaximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockXdgTopLevel>(resource)->UnsetMaximized();
}

void SetFullscreen(wl_client* client,
                   wl_resource* resource,
                   wl_resource* output) {
  GetUserDataAs<MockXdgTopLevel>(resource)->SetFullscreen();
}

void UnsetFullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockXdgTopLevel>(resource)->UnsetFullscreen();
}

void SetMinimized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockXdgTopLevel>(resource)->SetMinimized();
}

void SetMaxSize(wl_client* client,
                wl_resource* resource,
                int32_t width,
                int32_t height) {
  auto* toplevel = GetUserDataAs<MockXdgTopLevel>(resource);
  toplevel->SetMaxSize(width, height);
  // As it this can be envoked during construction of the XdgSurface, cache the
  // result so that tests are able to access that information.
  toplevel->set_max_size(gfx::Size(width, height));
}

void SetMinSize(wl_client* client,
                wl_resource* resource,
                int32_t width,
                int32_t height) {
  auto* toplevel = GetUserDataAs<MockXdgTopLevel>(resource);
  toplevel->SetMinSize(width, height);
  // As it this can be envoked during construction of the XdgSurface, cache the
  // result so that tests are able to access that information.
  toplevel->set_min_size(gfx::Size(width, height));
}

void GetTopLevel(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* surface = GetUserDataAs<MockXdgSurface>(resource);
  if (surface->xdg_toplevel()) {
    wl_resource_post_error(resource, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }
  wl_resource* xdg_toplevel_resource =
      wl_resource_create(client, &xdg_toplevel_interface, 1, id);
  if (!xdg_toplevel_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  surface->set_xdg_toplevel(
      std::make_unique<testing::NiceMock<MockXdgTopLevel>>(
          xdg_toplevel_resource, &kMockXdgToplevelImpl));
}

void GetXdgPopup(struct wl_client* client,
                 struct wl_resource* resource,
                 uint32_t id,
                 struct wl_resource* parent,
                 struct wl_resource* positioner_resource) {
  auto* mock_xdg_surface = GetUserDataAs<MockXdgSurface>(resource);
  wl_resource* current_resource = mock_xdg_surface->resource();
  if (current_resource &&
      (ResourceHasImplementation(current_resource, &xdg_popup_interface,
                                 &kXdgPopupImpl) ||
       ResourceHasImplementation(current_resource, &xdg_positioner_interface,
                                 &kTestXdgPositionerImpl))) {
    wl_resource_post_error(resource, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  wl_resource* xdg_popup_resource =
      CreateResourceWithImpl<::testing::NiceMock<TestXdgPopup>>(
          client, &xdg_popup_interface, wl_resource_get_version(resource),
          &kXdgPopupImpl, id, resource);

  if (!xdg_popup_resource) {
    wl_client_post_no_memory(client);
    return;
  }

  auto* test_xdg_popup = GetUserDataAs<TestXdgPopup>(xdg_popup_resource);
  DCHECK(test_xdg_popup);

  auto* positioner = GetUserDataAs<TestPositioner>(positioner_resource);
  DCHECK(positioner);

  test_xdg_popup->set_position(positioner->position());
  if (test_xdg_popup->size().IsEmpty() ||
      test_xdg_popup->anchor_rect().IsEmpty()) {
    wl_resource_post_error(resource, XDG_WM_BASE_ERROR_INVALID_POSITIONER,
                           "Positioner object is not complete");
    return;
  }

  mock_xdg_surface->set_xdg_popup(test_xdg_popup);
}

const struct xdg_surface_interface kMockXdgSurfaceImpl = {
    &DestroyResource,    // destroy
    &GetTopLevel,        // get_toplevel
    &GetXdgPopup,        // get_popup
    &SetWindowGeometry,  // set_window_geometry
    &AckConfigure,       // ack_configure
};

const struct xdg_toplevel_interface kMockXdgToplevelImpl = {
    &DestroyResource,  // destroy
    nullptr,           // set_parent
    &SetTitle,         // set_title
    &SetAppId,         // set_app_id
    nullptr,           // show_window_menu
    &Move,             // move
    &Resize,           // resize
    &SetMaxSize,       // set_max_size
    &SetMinSize,       // set_min_size
    &SetMaximized,     // set_maximized
    &UnsetMaximized,   // set_unmaximized
    &SetFullscreen,    // set_fullscreen
    &UnsetFullscreen,  // unset_fullscreen
    &SetMinimized,     // set_minimized
};

MockXdgSurface::MockXdgSurface(wl_resource* resource, wl_resource* surface)
    : ServerObject(resource), surface_(surface) {}

MockXdgSurface::~MockXdgSurface() {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface_);
  if (mock_surface)
    mock_surface->set_xdg_surface(nullptr);
}

MockXdgTopLevel::MockXdgTopLevel(wl_resource* resource,
                                 const void* implementation)
    : ServerObject(resource) {
  SetImplementationUnretained(resource, implementation, this);
}

MockXdgTopLevel::~MockXdgTopLevel() {}

}  // namespace wl
