// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_overlay_prioritizer.h"

#include <overlay-prioritizer-server-protocol.h>

#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_overlay_prioritized_surface.h"

namespace wl {

namespace {

constexpr uint32_t kOverlayPriortizerProtocolVersion = 1;

void GetOverlayPrioritizedSurface(struct wl_client* client,
                                  struct wl_resource* resource,
                                  uint32_t id,
                                  struct wl_resource* surface) {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface);
  if (mock_surface->prioritized_surface()) {
    wl_resource_post_error(
        resource, OVERLAY_PRIORITIZER_ERROR_OVERLAY_HINTED_SURFACE_EXISTS,
        "overlay_prioritizer exists");
    return;
  }

  wl_resource* prioritized_surface_resource = CreateResourceWithImpl<
      ::testing::NiceMock<TestOverlayPrioritizedSurface>>(
      client, &overlay_prioritized_surface_interface,
      wl_resource_get_version(resource), &kTestOverlayPrioritizedSurfaceImpl,
      id, surface);
  DCHECK(prioritized_surface_resource);
  mock_surface->set_overlay_prioritized_surface(
      GetUserDataAs<TestOverlayPrioritizedSurface>(
          prioritized_surface_resource));
}

}  // namespace

const struct overlay_prioritizer_interface kTestOverlayPrioritizerImpl = {
    DestroyResource,
    GetOverlayPrioritizedSurface,
};

TestOverlayPrioritizer::TestOverlayPrioritizer()
    : GlobalObject(&overlay_prioritizer_interface,
                   &kTestOverlayPrioritizerImpl,
                   kOverlayPriortizerProtocolVersion) {}

TestOverlayPrioritizer::~TestOverlayPrioritizer() = default;

}  // namespace wl
