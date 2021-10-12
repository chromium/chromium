// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_augmented_surface.h"

#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"

namespace wl {

namespace {

void SetRoundedCorners(struct wl_client* client,
                       struct wl_resource* resource,
                       wl_fixed_t top_left,
                       wl_fixed_t top_right,
                       wl_fixed_t bottom_right,
                       wl_fixed_t bottom_left) {
  GetUserDataAs<TestAugmentedSurface>(resource)->set_rounded_corners(
      gfx::RoundedCornersF(
          wl_fixed_to_double(top_left), wl_fixed_to_double(top_right),
          wl_fixed_to_double(bottom_right), wl_fixed_to_double(bottom_left)));
}

}  // namespace

const struct augmented_surface_interface kTestAugmentedSurfaceImpl = {
    DestroyResource,
    SetRoundedCorners,
};

TestAugmentedSurface::TestAugmentedSurface(wl_resource* resource,
                                           wl_resource* surface)
    : ServerObject(resource), surface_(surface) {
  DCHECK(surface_);
}

TestAugmentedSurface::~TestAugmentedSurface() {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface_);
  if (mock_surface)
    mock_surface->set_augmented_surface(nullptr);
}

}  // namespace wl
