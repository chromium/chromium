// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_compositor.h"

#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_region.h"

namespace wl {

namespace {

void CreateSurface(wl_client* client,
                   wl_resource* compositor_resource,
                   uint32_t id) {
  wl_resource* resource =
      CreateResourceWithImpl<::testing::NiceMock<MockSurface>>(
          client, &wl_surface_interface,
          wl_resource_get_version(compositor_resource), &kMockSurfaceImpl, id);
  GetUserDataAs<TestCompositor>(compositor_resource)
      ->AddSurface(GetUserDataAs<MockSurface>(resource));
}

void CreateRegion(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* region_resource =
      wl_resource_create(client, &wl_region_interface, 1, id);
  SetImplementation(region_resource, &kTestWlRegionImpl,
                    std::make_unique<TestRegion>());
}

}  // namespace

const struct wl_compositor_interface kTestCompositorImpl = {
    CreateSurface,  // create_surface
    CreateRegion,   // create_region
};

TestCompositor::TestCompositor(TestCompositor::Version intended_version)
    : GlobalObject(&wl_compositor_interface,
                   &kTestCompositorImpl,
                   static_cast<uint32_t>(intended_version)),
      version_(intended_version) {}

TestCompositor::~TestCompositor() = default;

void TestCompositor::AddSurface(MockSurface* surface) {
  surfaces_.push_back(surface);
}

}  // namespace wl
