// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zaura_popup.h"

#include <aura-shell-server-protocol.h>

#include "base/notreached.h"

namespace wl {

namespace {

void SurfaceSubmissionInPixelCoordinates(struct wl_client* client,
                                         struct wl_resource* resource) {
  // TODO(crbug.com/40232463): Implement zaura-shell protocol requests and test
  // their usage.
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetDecoration(struct wl_client* client,
                   struct wl_resource* resource,
                   uint32_t type) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetMenu(struct wl_client* client, struct wl_resource* resource) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace

TestZAuraPopup::TestZAuraPopup(wl_resource* resource)
    : ServerObject(resource) {}

TestZAuraPopup::~TestZAuraPopup() = default;

const struct zaura_popup_interface kTestZAuraPopupImpl = {
    &SurfaceSubmissionInPixelCoordinates,
    &SetDecoration,
    &SetMenu,
    &DestroyResource,
};

}  // namespace wl
