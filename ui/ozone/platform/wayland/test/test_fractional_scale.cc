// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_fractional_scale.h"

#include <fractional-scale-v1-server-protocol.h>

#include "base/check.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"

namespace wl {

namespace {

constexpr uint32_t kTestFractionalScaleManagerVersion = 1;
constexpr struct wp_fractional_scale_v1_interface kTestFractionalScaleImpl = {
    .destroy = DestroyResource};

void GetFractionalScale(struct wl_client* client,
                        struct wl_resource* resource,
                        uint32_t id,
                        struct wl_resource* surface) {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface);
  if (mock_surface->fractional_scale()) {
    wl_resource_post_error(
        resource, WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS,
        "fractional scale exists");
    return;
  }

  wl_resource* fractional_scale_resource =
      CreateResourceWithImpl<::testing::NiceMock<TestFractionalScale>>(
          client, &wp_fractional_scale_v1_interface,
          wl_resource_get_version(resource), &kTestFractionalScaleImpl, id,
          surface);
  CHECK(fractional_scale_resource);
  mock_surface->set_fractional_scale(
      GetUserDataAs<TestFractionalScale>(fractional_scale_resource));
}

constexpr struct wp_fractional_scale_manager_v1_interface
    kTestFractionalScaleManagerImpl = {
        .destroy = DestroyResource,
        .get_fractional_scale = GetFractionalScale,
};

}  // namespace

TestFractionalScaleManager::TestFractionalScaleManager()
    : GlobalObject(&wp_fractional_scale_manager_v1_interface,
                   &kTestFractionalScaleManagerImpl,
                   kTestFractionalScaleManagerVersion) {}

TestFractionalScaleManager::~TestFractionalScaleManager() = default;

TestFractionalScale::TestFractionalScale(wl_resource* resource,
                                         wl_resource* surface)
    : ServerObject(resource), surface_(surface) {
  CHECK(surface_);
}

TestFractionalScale::~TestFractionalScale() {
  if (auto* mock_surface = GetUserDataAs<MockSurface>(surface_)) {
    mock_surface->set_fractional_scale(nullptr);
  }
}

void TestFractionalScale::SendPreferredScale(float scale) {
  const uint32_t scale_int = scale == 1.0f ? 0 : static_cast<int>(scale * 120);
  wp_fractional_scale_v1_send_preferred_scale(resource(), scale_int);
}

}  // namespace wl
