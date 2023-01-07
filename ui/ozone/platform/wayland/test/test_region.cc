// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_region.h"

#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

void Destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void Add(wl_client* client,
         wl_resource* resource,
         int32_t x,
         int32_t y,
         int32_t width,
         int32_t height) {
  GetUserDataAs<SkRegion>(resource)->op(SkIRect::MakeXYWH(x, y, width, height),
                                        SkRegion::kUnion_Op);
}

static void Subtract(wl_client* client,
                     wl_resource* resource,
                     int32_t x,
                     int32_t y,
                     int32_t width,
                     int32_t height) {
  GetUserDataAs<SkRegion>(resource)->op(SkIRect::MakeXYWH(x, y, width, height),
                                        SkRegion::kDifference_Op);
}

}  // namespace

const struct wl_region_interface kTestWlRegionImpl = {Destroy, Add, Subtract};

}  // namespace wl
