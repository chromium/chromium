// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_alpha_compositing.h"

#include <alpha-compositing-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include "base/check.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_alpha_blending.h"

namespace wl {

namespace {

constexpr uint32_t kAlphaCompositingVersion = 1;

void GetBlending(struct wl_client* client,
                 struct wl_resource* resource,
                 uint32_t id,
                 struct wl_resource* surface) {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface);
  if (mock_surface->blending()) {
    wl_resource_post_error(resource,
                           ZCR_ALPHA_COMPOSITING_V1_ERROR_BLENDING_EXISTS,
                           "Alpha Compositing exists");
    return;
  }

  wl_resource* blending_resource =
      CreateResourceWithImpl<::testing::NiceMock<TestAlphaBlending>>(
          client, &zcr_blending_v1_interface, wl_resource_get_version(resource),
          &kTestAlphaBlendingImpl, id, surface);
  DCHECK(blending_resource);
  mock_surface->set_blending(
      GetUserDataAs<TestAlphaBlending>(blending_resource));
}

}  // namespace

const struct zcr_alpha_compositing_v1_interface kTestAlphaCompositingImpl = {
    DestroyResource,
    GetBlending,
};

TestAlphaCompositing::TestAlphaCompositing()
    : GlobalObject(&zcr_alpha_compositing_v1_interface,
                   &kTestAlphaCompositingImpl,
                   kAlphaCompositingVersion) {}

TestAlphaCompositing::~TestAlphaCompositing() = default;

}  // namespace wl
