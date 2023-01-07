// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_SURFACE_H_

#include <aura-shell-server-protocol.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

extern const struct zaura_surface_interface kTestZAuraSurfaceImpl;

// Manages zaura_surface object.
class TestZAuraSurface : public ServerObject {
 public:
  explicit TestZAuraSurface(wl_resource* resource);

  TestZAuraSurface(const TestZAuraSurface&) = delete;
  TestZAuraSurface& operator=(const TestZAuraSurface&) = delete;

  ~TestZAuraSurface() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_SURFACE_H_
