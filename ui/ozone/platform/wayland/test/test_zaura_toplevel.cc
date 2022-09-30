// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zaura_toplevel.h"

#include <aura-shell-server-protocol.h>

#include "base/notreached.h"

namespace wl {

namespace {

void SetOrientationLock(struct wl_client* client,
                        struct wl_resource* resource,
                        uint32_t orientation_lock) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SurfaceSubmissionInPixelCoordinates(struct wl_client* client,
                                         struct wl_resource* resource) {
  // TODO(crbug.com/1346347): Implement zaura-shell protocol requests and test
  // their usage.
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetSupportsScreenCoordinates(struct wl_client* client,
                                  struct wl_resource* resource) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetWindowBounds(struct wl_client* client,
                     struct wl_resource* resource,
                     int32_t x,
                     int32_t y,
                     int32_t width,
                     int32_t height,
                     struct wl_resource* output) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetOrigin(struct wl_client* client,
               struct wl_resource* resource,
               int32_t x,
               int32_t y,
               struct wl_resource* output) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetRestoreInfo(struct wl_client* client,
                    struct wl_resource* resource,
                    int32_t restore_session_id,
                    int32_t restore_window_id) {
  NOTREACHED();
}

void SetSystemModal(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED();
}

void UnsetSystemModal(struct wl_client* client, struct wl_resource* resource) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetRestoreInfoWithWindowIdSource(struct wl_client* client,
                                      struct wl_resource* resource,
                                      int32_t restore_session_id,
                                      const char* restore_window_id_source) {
  NOTREACHED();
}

void SetDecoration(struct wl_client* client,
                   struct wl_resource* resource,
                   uint32_t type) {
  NOTREACHED();
}

void SetFloat(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED();
}

void UnSetFloat(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED();
}

void SetZOrder(struct wl_client* client,
               struct wl_resource* resource,
               uint32_t z_order) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void Activate(struct wl_client* client, struct wl_resource* resource) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void Dectivate(struct wl_client* client, struct wl_resource* resource) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetFullscreenMode(struct wl_client* client,
                       struct wl_resource* resource,
                       uint32_t mode) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace

TestZAuraToplevel::TestZAuraToplevel(wl_resource* resource)
    : ServerObject(resource) {}

TestZAuraToplevel::~TestZAuraToplevel() = default;

const struct zaura_toplevel_interface kTestZAuraToplevelImpl = {
    &SetOrientationLock,
    &SurfaceSubmissionInPixelCoordinates,
    &SetSupportsScreenCoordinates,
    &SetWindowBounds,
    &SetRestoreInfo,
    &SetSystemModal,
    &UnsetSystemModal,
    &SetRestoreInfoWithWindowIdSource,
    &SetDecoration,
    &DestroyResource,
    &SetFloat,
    &UnSetFloat,
    &SetZOrder,
    &SetOrigin,
    &Activate,
    &Dectivate,
    &SetFullscreenMode,
};

}  // namespace wl
