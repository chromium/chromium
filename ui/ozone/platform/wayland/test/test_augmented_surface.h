// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_AUGMENTED_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_AUGMENTED_SURFACE_H_

#include <surface-augmenter-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct augmented_surface_interface kTestAugmentedSurfaceImpl;

class TestAugmentedSurface : public ServerObject {
 public:
  TestAugmentedSurface(wl_resource* resource, wl_resource* surface);
  ~TestAugmentedSurface() override;
  TestAugmentedSurface(const TestAugmentedSurface& rhs) = delete;
  TestAugmentedSurface& operator=(const TestAugmentedSurface& rhs) = delete;

  void set_rounded_clip_bounds(const gfx::RRectF& rounded_clip_bounds) {
    rounded_clip_bounds_ = rounded_clip_bounds;
  }
  const gfx::RRectF& rounded_clip_bounds() { return rounded_clip_bounds_; }

  wl_resource* surface() const { return surface_; }

 private:
  // Surface resource that is the ground for this augmented surface.
  raw_ptr<wl_resource> surface_ = nullptr;

  gfx::RRectF rounded_clip_bounds_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_AUGMENTED_SURFACE_H_
