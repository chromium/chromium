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
  // TODO(crbug.com/40232463): Implement zaura-shell protocol requests and test
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
  NOTREACHED_IN_MIGRATION();
}

void SetSystemModal(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}

void UnsetSystemModal(struct wl_client* client, struct wl_resource* resource) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetRestoreInfoWithWindowIdSource(struct wl_client* client,
                                      struct wl_resource* resource,
                                      int32_t restore_session_id,
                                      const char* restore_window_id_source) {
  NOTREACHED_IN_MIGRATION();
}

void SetDecoration(struct wl_client* client,
                   struct wl_resource* resource,
                   uint32_t type) {
  NOTREACHED_IN_MIGRATION();
}

void SetFloat(struct wl_client* client, struct wl_resource* resource) {
  NOTREACHED_IN_MIGRATION();
}

void UnSetFloat(struct wl_client* client, struct wl_resource* resource) {
  auto* toplevel = GetUserDataAs<TestZAuraToplevel>(resource);
  if (toplevel->set_unset_float_callback()) {
    toplevel->set_unset_float_callback().Run(/*floated=*/false, 0);
  } else {
    NOTIMPLEMENTED_LOG_ONCE();
  }
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

void SetScaleFactor(wl_client* client,
                    wl_resource* resource,
                    uint32_t scale_factor_as_uint) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetSnapPrimary(wl_client* client,
                    wl_resource* resource,
                    uint32_t snap_ratio_as_uint) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetSnapSecondary(wl_client* client,
                      wl_resource* resource,
                      uint32_t snap_ratio_as_uint) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void IntentToSnap(wl_client* client,
                  wl_resource* resource,
                  uint32_t snap_direction) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void UnsetSnap(wl_client* client, wl_resource* resource) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetPersistable(wl_client* client,
                    wl_resource* resource,
                    uint32_t persistable) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetShape(wl_client* client,
              wl_resource* resource,
              wl_resource* region_resource) {
  GetUserDataAs<TestZAuraToplevel>(resource)->set_shape(
      region_resource ? std::optional<TestRegion>(
                            *GetUserDataAs<TestRegion>(region_resource))
                      : std::nullopt);
}

void SetTopInset(wl_client* client, wl_resource* resource, int32_t height) {
  GetUserDataAs<TestZAuraToplevel>(resource)->set_top_inset(height);
}

void AckRotateFocus(wl_client* client,
                    wl_resource* resource,
                    uint32_t serial,
                    uint32_t handled) {
  auto* toplevel = GetUserDataAs<TestZAuraToplevel>(resource);
  if (toplevel->ack_rotate_focus_callback()) {
    toplevel->ack_rotate_focus_callback().Run(serial, handled);
  } else {
    NOTIMPLEMENTED_LOG_ONCE();
  }
}

void SetCanMaximize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TestZAuraToplevel>(resource)->set_can_maximize(true);
}

void UnsetCanMaximize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TestZAuraToplevel>(resource)->set_can_maximize(false);
}

void SetCanFullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TestZAuraToplevel>(resource)->set_can_fullscreen(true);
}

void UnsetCanFullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TestZAuraToplevel>(resource)->set_can_fullscreen(false);
}

void SetFloatToLocation(struct wl_client* client,
                        struct wl_resource* resource,
                        uint32_t float_start_location) {
  auto* toplevel = GetUserDataAs<TestZAuraToplevel>(resource);
  if (toplevel->set_unset_float_callback()) {
    toplevel->set_unset_float_callback().Run(/*floated=*/true,
                                             float_start_location);
  } else {
    NOTIMPLEMENTED_LOG_ONCE();
  }
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
    &SetScaleFactor,
    &SetSnapPrimary,
    &SetSnapSecondary,
    &IntentToSnap,
    &UnsetSnap,
    &SetPersistable,
    &SetShape,
    &SetTopInset,
    &AckRotateFocus,
    &SetCanMaximize,
    &UnsetCanMaximize,
    &SetCanFullscreen,
    &UnsetCanFullscreen,
    &SetFloatToLocation,
};

}  // namespace wl
