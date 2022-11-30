// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_augmented_subsurface.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"

namespace wl {

namespace {

void SetPosition(struct wl_client* client,
                 struct wl_resource* resource,
                 wl_fixed_t x,
                 wl_fixed_t y) {
  auto* test_augmented_subsurface =
      GetUserDataAs<TestAugmentedSubSurface>(resource);
  DCHECK(test_augmented_subsurface);

  auto* test_subsurface =
      GetUserDataAs<TestSubSurface>(test_augmented_subsurface->sub_surface());
  DCHECK(test_subsurface);
  test_subsurface->SetPositionImpl(wl_fixed_to_double(x),
                                   wl_fixed_to_double(y));
}

}  // namespace

const struct augmented_sub_surface_interface kTestAugmentedSubSurfaceImpl = {
    DestroyResource,
    SetPosition,
};

TestAugmentedSubSurface::TestAugmentedSubSurface(wl_resource* resource,
                                                 wl_resource* sub_surface)
    : ServerObject(resource), sub_surface_(sub_surface) {
  DCHECK(sub_surface_);
}

TestAugmentedSubSurface::~TestAugmentedSubSurface() {
  auto* test_sub_surface = GetUserDataAs<TestSubSurface>(sub_surface_);
  if (test_sub_surface)
    test_sub_surface->set_augmented_subsurface(nullptr);
}

}  // namespace wl
