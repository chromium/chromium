// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_subcompositor.h"

#include <wayland-server-core.h>

#include "base/check.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_subsurface.h"

namespace wl {

namespace {

constexpr uint32_t kSubCompositorVersion = 1;

void GetSubsurface(struct wl_client* client,
                   struct wl_resource* resource,
                   uint32_t id,
                   struct wl_resource* surface,
                   struct wl_resource* parent) {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface);
  auto* parent_surface = GetUserDataAs<MockSurface>(parent);
  if (mock_surface->has_role() || !parent_surface) {
    wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                           "invalid surface");
    return;
  }

  wl_resource* subsurface_resource =
      CreateResourceWithImpl<::testing::NiceMock<TestSubSurface>>(
          client, &wl_subsurface_interface, wl_resource_get_version(resource),
          &kTestSubSurfaceImpl, id, surface, parent);
  DCHECK(subsurface_resource);
  mock_surface->set_sub_surface(
      GetUserDataAs<TestSubSurface>(subsurface_resource));
}

}  // namespace

const struct wl_subcompositor_interface kTestSubCompositorImpl = {
    DestroyResource,
    GetSubsurface,
};

TestSubCompositor::TestSubCompositor()
    : GlobalObject(&wl_subcompositor_interface,
                   &kTestSubCompositorImpl,
                   kSubCompositorVersion) {}

TestSubCompositor::~TestSubCompositor() {}

}  // namespace wl
