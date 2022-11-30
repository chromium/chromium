// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_alpha_compositing.h"

#include <alpha-compositing-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include "base/check.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_viewport.h"

namespace wl {

namespace {

void SetBlending(wl_client* client, wl_resource* resource, uint32_t equation) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetAlpha(wl_client* client, wl_resource* resource, wl_fixed_t value) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace

const struct zcr_blending_v1_interface kTestAlphaBlendingImpl = {
    DestroyResource,
    SetBlending,
    SetAlpha,
};

TestAlphaBlending::TestAlphaBlending(wl_resource* resource,
                                     wl_resource* surface)
    : ServerObject(resource), surface_(surface) {
  DCHECK(surface_);
}

TestAlphaBlending::~TestAlphaBlending() {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface_);
  if (mock_surface)
    mock_surface->set_blending(nullptr);
}

}  // namespace wl
