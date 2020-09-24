// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_viewport.h"

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"

namespace wl {

namespace {

void SetSource(wl_client* client,
               wl_resource* resource,
               wl_fixed_t x,
               wl_fixed_t y,
               wl_fixed_t width,
               wl_fixed_t height) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetDestination(wl_client* client,
                    wl_resource* resource,
                    int32_t x,
                    int32_t y) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace

const struct wp_viewport_interface kTestViewportImpl = {
    DestroyResource,
    SetSource,
    SetDestination,
};

TestViewport::TestViewport(wl_resource* resource, wl_resource* surface)
    : ServerObject(resource), surface_(surface) {
  DCHECK(surface_);
}

TestViewport::~TestViewport() {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface_);
  if (mock_surface)
    mock_surface->set_viewport(nullptr);
}

}  // namespace wl
