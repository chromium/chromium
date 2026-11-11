// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_xdg_toplevel_drag.h"

#include <wayland-server-core.h>

#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

void GetXdgToplevelDrag(struct wl_client* client,
                        struct wl_resource* resource,
                        uint32_t id,
                        struct wl_resource* data_source) {
  auto* global = GetUserDataAs<MockXdgToplevelDragManagerV1>(resource);
  // No need to pass an on_resource_destroyed callback here, as the
  // ~MockXdgToplevelDragV1() takes cares of the cleanup.
  wl_resource* drag = CreateResourceWithImpl<MockXdgToplevelDragV1>(
      client, &xdg_toplevel_drag_v1_interface, 1, &kMockXdgToplevelDragImpl, id,
      base::DoNothing(), global);
  global->set_drag(GetUserDataAs<MockXdgToplevelDragV1>(drag));
}

}  // namespace

const struct xdg_toplevel_drag_manager_v1_interface
    kMockXdgToplevelDragManagerImpl = {
        .destroy = DestroyResource,
        .get_xdg_toplevel_drag = GetXdgToplevelDrag,
};

MockXdgToplevelDragManagerV1::MockXdgToplevelDragManagerV1()
    : GlobalObject(&xdg_toplevel_drag_manager_v1_interface,
                   &kMockXdgToplevelDragManagerImpl,
                   1) {}

MockXdgToplevelDragManagerV1::~MockXdgToplevelDragManagerV1() = default;

namespace {

void Attach(struct wl_client* client,
            struct wl_resource* resource,
            struct wl_resource* toplevel,
            int32_t x_offset,
            int32_t y_offset) {
  GetUserDataAs<MockXdgToplevelDragV1>(resource)->set_attached_offset(
      {x_offset, y_offset});
}

}  // namespace

const struct xdg_toplevel_drag_v1_interface kMockXdgToplevelDragImpl = {
    .destroy = DestroyResource,
    .attach = Attach,
};

MockXdgToplevelDragV1::MockXdgToplevelDragV1(
    wl_resource* resource,
    MockXdgToplevelDragManagerV1* global)
    : ServerObject(resource), global_(global) {}

MockXdgToplevelDragV1::~MockXdgToplevelDragV1() {
  if (global_->drag() == this) {
    global_->set_drag(nullptr);
  }
}

}  // namespace wl
