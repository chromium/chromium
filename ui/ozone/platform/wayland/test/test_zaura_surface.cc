// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zaura_surface.h"

#include <aura-shell-server-protocol.h>

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

// TODO(crbug.com/40881920): Replace NOTREACHED() by NOTIMPLEMENTED() for
// all methods?
void set_frame(struct wl_client* client,
               struct wl_resource* resource,
               uint32_t type) {
  NOTREACHED_IN_MIGRATION();
}
void set_parent(struct wl_client* client,
                struct wl_resource* resource,
                struct wl_resource* parent,
                int32_t x,
                int32_t y) {
  NOTREACHED_IN_MIGRATION();
}
void set_frame_colors(struct wl_client* client,
                      struct wl_resource* resource,
                      uint32_t active_color,
                      uint32_t inactive_color) {
  NOTREACHED_IN_MIGRATION();
}
void set_startup_id(struct wl_client* client,
                    struct wl_resource* resource,
                    const char* startup_id) {
  NOTREACHED_IN_MIGRATION();
}
void set_application_id(struct wl_client* client,
                        struct wl_resource* resource,
                        const char* application_id) {
  NOTREACHED_IN_MIGRATION();
}
void set_client_surface_id(struct wl_client* client,
                           struct wl_resource* resource,
                           int32_t client_surface_id) {
  NOTREACHED_IN_MIGRATION();
}
void set_occlusion_tracking(struct wl_client* client,
                            struct wl_resource* resource) {
  NOTIMPLEMENTED_LOG_ONCE();
}
void unset_occlusion_tracking(struct wl_client* client,
                              struct wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}
void activate(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}
void draw_attention(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}
void set_fullscreen_mode(struct wl_client* client,
                         struct wl_resource* resource,
                         uint32_t mode) {
  NOTIMPLEMENTED_LOG_ONCE();
}
void set_client_surface_str_id(struct wl_client* client,
                               struct wl_resource* resource,
                               const char* client_surface_id) {
  NOTREACHED_IN_MIGRATION();
}
void set_server_start_resize(struct wl_client* client,
                             struct wl_resource* resource) {
  NOTIMPLEMENTED_LOG_ONCE();
}
void intent_to_snap(struct wl_client* client,
                    struct wl_resource* resource,
                    uint32_t direction) {
  NOTREACHED_IN_MIGRATION();
}
void set_snap_left(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}
void set_snap_right(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}
void unset_snap(struct wl_client* client, struct wl_resource* resource) {
  NOTIMPLEMENTED_LOG_ONCE();
}
void set_window_session_id(struct wl_client* client,
                           struct wl_resource* resource,
                           int32_t id) {
  NOTREACHED_IN_MIGRATION();
}
void set_can_go_back(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}
void unset_can_go_back(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}
void set_pip(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}
void unset_pip(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}
void set_aspect_ratio(struct wl_client* client,
                      struct wl_resource* resource,
                      int32_t width,
                      int32_t height) {
  NOTREACHED_IN_MIGRATION();
}
void move_to_desk(struct wl_client* client,
                  struct wl_resource* resource,
                  int32_t index) {
  NOTREACHED_IN_MIGRATION();
}
void set_initial_workspace(struct wl_client* client,
                           struct wl_resource* resource,
                           const char* initial_workspace) {
  NOTREACHED_IN_MIGRATION();
}
void set_pin(struct wl_client* client,
             struct wl_resource* resource,
             int32_t trusted) {
  NOTREACHED_IN_MIGRATION();
}
void unset_pin(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}
}  // namespace

TestZAuraSurface::TestZAuraSurface(wl_resource* resource)
    : ServerObject(resource) {}

TestZAuraSurface::~TestZAuraSurface() = default;

const struct zaura_surface_interface kTestZAuraSurfaceImpl = {
    &set_frame,
    &set_parent,
    &set_frame_colors,
    &set_startup_id,
    &set_application_id,
    &set_client_surface_id,
    &set_occlusion_tracking,
    &unset_occlusion_tracking,
    &activate,
    &draw_attention,
    &set_fullscreen_mode,
    &set_client_surface_str_id,
    &set_server_start_resize,
    &intent_to_snap,
    &set_snap_left,
    &set_snap_right,
    &unset_snap,
    &set_window_session_id,
    &set_can_go_back,
    &unset_can_go_back,
    &set_pip,
    &unset_pip,
    &set_aspect_ratio,
    &move_to_desk,
    &set_initial_workspace,
    &set_pin,
    &unset_pin,
    &DestroyResource,
};

}  // namespace wl
