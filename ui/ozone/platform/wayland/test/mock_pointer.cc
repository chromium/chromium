// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_pointer.h"

namespace wl {

namespace {

void SetCursor(wl_client* client,
               wl_resource* pointer_resource,
               uint32_t serial,
               wl_resource* surface_resource,
               int32_t hotspot_x,
               int32_t hotspot_y) {
  GetUserDataAs<MockPointer>(pointer_resource)
      ->SetCursor(surface_resource, hotspot_x, hotspot_y);
}

}  // namespace

const struct wl_pointer_interface kMockPointerImpl = {
    &SetCursor,        // set_cursor
    &DestroyResource,  // release
};

MockPointer::MockPointer(wl_resource* resource) : ServerObject(resource) {}

MockPointer::~MockPointer() = default;

}  // namespace wl
