// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_REGION_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_REGION_H_

#include <wayland-server-protocol.h>

#include "third_party/skia/include/core/SkRegion.h"

namespace wl {

extern const struct wl_region_interface kTestWlRegionImpl;

using TestRegion = SkRegion;

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_REGION_H_
