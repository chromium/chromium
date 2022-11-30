// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_overlay_prioritized_surface.h"

#include "ui/ozone/platform/wayland/test/mock_surface.h"

namespace wl {

namespace {

void SetOverlayPriority(struct wl_client* client,
                        struct wl_resource* resource,
                        uint32_t priority) {
  GetUserDataAs<TestOverlayPrioritizedSurface>(resource)->set_overlay_priority(
      priority);
}

}  // namespace

const struct overlay_prioritized_surface_interface
    kTestOverlayPrioritizedSurfaceImpl = {
        DestroyResource,
        SetOverlayPriority,
};

TestOverlayPrioritizedSurface::TestOverlayPrioritizedSurface(
    wl_resource* resource,
    wl_resource* surface)
    : ServerObject(resource), surface_(surface) {
  DCHECK(surface_);
}

TestOverlayPrioritizedSurface::~TestOverlayPrioritizedSurface() {
  auto* mock_prioritized_surface = GetUserDataAs<MockSurface>(surface_);
  if (mock_prioritized_surface)
    mock_prioritized_surface->set_overlay_prioritized_surface(nullptr);
}

}  // namespace wl
