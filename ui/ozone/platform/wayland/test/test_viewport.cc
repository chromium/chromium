// Copyright 2020 The Chromium Authors
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
  auto* test_vp = GetUserDataAs<TestViewport>(resource);
  DCHECK(test_vp);
  test_vp->SetSource(wl_fixed_to_double(x), wl_fixed_to_double(y),
                     wl_fixed_to_double(width), wl_fixed_to_double(height));
}

void SetDestination(wl_client* client,
                    wl_resource* resource,
                    int32_t width,
                    int32_t height) {
  auto* test_vp = GetUserDataAs<TestViewport>(resource);
  DCHECK(test_vp);
  test_vp->SetDestinationImpl(width, height);
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

void TestViewport::SetDestinationImpl(float width, float height) {
  destination_size_ = gfx::SizeF(width, height);
  SetDestination(width, height);
}

}  // namespace wl
