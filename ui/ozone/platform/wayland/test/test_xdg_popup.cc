// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_xdg_popup.h"

#include "ui/ozone/platform/wayland/test/mock_xdg_surface.h"
#include "ui/ozone/platform/wayland/test/test_positioner.h"

namespace wl {

namespace {

void Grab(struct wl_client* client,
          struct wl_resource* resource,
          struct wl_resource* seat,
          uint32_t serial) {
  GetUserDataAs<TestXdgPopup>(resource)->set_grab_serial(serial);
}

void Reposition(struct wl_client* client,
                struct wl_resource* resource,
                struct wl_resource* positioner,
                uint32_t token) {
  auto* test_positioner = GetUserDataAs<TestPositioner>(positioner);
  DCHECK(test_positioner);
  GetUserDataAs<TestXdgPopup>(resource)->set_position(
      test_positioner->position());
}

}  // namespace

const struct xdg_popup_interface kXdgPopupImpl = {
    &DestroyResource,  // destroy
    &Grab,             // grab
    &Reposition,       // reposition
};

TestXdgPopup::TestXdgPopup(wl_resource* resource, wl_resource* surface)
    : ServerObject(resource), surface_(surface) {
  auto* mock_xdg_surface = GetUserDataAs<MockXdgSurface>(surface_);
  if (mock_xdg_surface)
    mock_xdg_surface->set_xdg_popup(nullptr);
}

TestXdgPopup::~TestXdgPopup() {}

}  // namespace wl
