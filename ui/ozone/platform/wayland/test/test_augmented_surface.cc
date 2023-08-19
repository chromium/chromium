// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_augmented_surface.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"

namespace wl {

namespace {

void SetRoundedCornersDEPRECATED(struct wl_client* client,
                                 struct wl_resource* resource,
                                 wl_fixed_t top_left,
                                 wl_fixed_t top_right,
                                 wl_fixed_t bottom_right,
                                 wl_fixed_t bottom_left) {
  LOG(ERROR) << "Deprecated.";
}

void SetDestinationSize(struct wl_client* client,
                        struct wl_resource* resource,
                        wl_fixed_t width,
                        wl_fixed_t height) {
  auto* res = GetUserDataAs<TestAugmentedSurface>(resource)->surface();
  DCHECK(res);

  auto* mock_surface = GetUserDataAs<MockSurface>(res);

  auto* viewport = mock_surface->viewport();
  DCHECK(viewport);
  viewport->SetDestinationImpl(wl_fixed_to_double(width),
                               wl_fixed_to_double(height));
}

void SetRoundedClipBoundsDEPRECATED(struct wl_client* client,
                                    struct wl_resource* resource,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    wl_fixed_t top_left,
                                    wl_fixed_t top_right,
                                    wl_fixed_t bottom_right,
                                    wl_fixed_t bottom_left) {
  LOG(ERROR) << "Deprecated.";
}

void SetBackgroundColor(wl_client* client,
                        wl_resource* resource,
                        wl_array* color_data) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetTrustedDamage(wl_client* client, wl_resource* resource, int enabled) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetRoundedCornersClipBounds(wl_client* client,
                                 wl_resource* resource,
                                 wl_fixed_t x,
                                 wl_fixed_t y,
                                 wl_fixed_t width,
                                 wl_fixed_t height,
                                 wl_fixed_t top_left,
                                 wl_fixed_t top_right,
                                 wl_fixed_t bottom_right,
                                 wl_fixed_t bottom_left) {
  GetUserDataAs<TestAugmentedSurface>(resource)->set_rounded_clip_bounds(
      gfx::RRectF(
          gfx::RectF(wl_fixed_to_double(x), wl_fixed_to_double(y),
                     wl_fixed_to_double(width), wl_fixed_to_double(height)),
          gfx::RoundedCornersF(wl_fixed_to_double(top_left),
                               wl_fixed_to_double(top_right),
                               wl_fixed_to_double(bottom_right),
                               wl_fixed_to_double(bottom_left))));
}

}  // namespace

const struct augmented_surface_interface kTestAugmentedSurfaceImpl = {
    DestroyResource,
    SetRoundedCornersDEPRECATED,
    SetDestinationSize,
    SetRoundedClipBoundsDEPRECATED,
    SetBackgroundColor,
    SetTrustedDamage,
    SetRoundedCornersClipBounds,
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
