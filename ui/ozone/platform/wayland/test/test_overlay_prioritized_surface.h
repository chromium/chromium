// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OVERLAY_PRIORITIZED_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OVERLAY_PRIORITIZED_SURFACE_H_

#include <overlay-prioritizer-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct overlay_prioritized_surface_interface
    kTestOverlayPrioritizedSurfaceImpl;

class TestOverlayPrioritizedSurface : public ServerObject {
 public:
  TestOverlayPrioritizedSurface(wl_resource* resource, wl_resource* surface);
  ~TestOverlayPrioritizedSurface() override;
  TestOverlayPrioritizedSurface(const TestOverlayPrioritizedSurface& rhs) =
      delete;
  TestOverlayPrioritizedSurface& operator=(
      const TestOverlayPrioritizedSurface& rhs) = delete;

  void set_overlay_priority(uint32_t priority) { overlay_priority_ = priority; }
  uint32_t overlay_priority() { return overlay_priority_; }

 private:
  // Surface resource that is the ground for this prioritized surface.
  raw_ptr<wl_resource> surface_ = nullptr;

  uint32_t overlay_priority_ =
      OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_NONE;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OVERLAY_PRIORITIZED_SURFACE_H_
