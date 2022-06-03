// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_surface.h"

#include "base/notreached.h"

namespace wl {

namespace {

void SetPosition(wl_client* client,
                 wl_resource* resource,
                 int32_t x,
                 int32_t y) {
  GetUserDataAs<TestSubSurface>(resource)->SetPosition(x, y);
}

void PlaceAbove(wl_client* client,
                wl_resource* resource,
                wl_resource* reference_resource) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlaceBelow(wl_client* client,
                wl_resource* resource,
                wl_resource* sibling_resource) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SetSync(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TestSubSurface>(resource)->set_sync(true);
}

void SetDesync(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TestSubSurface>(resource)->set_sync(false);
}

}  // namespace

const struct wl_subsurface_interface kTestSubSurfaceImpl = {
    DestroyResource, SetPosition, PlaceAbove, PlaceBelow, SetSync, SetDesync,
};

TestSubSurface::TestSubSurface(wl_resource* resource,
                               wl_resource* surface,
                               wl_resource* parent_resource)
    : ServerObject(resource),
      surface_(surface),
      parent_resource_(parent_resource) {
  DCHECK(surface_);
}

TestSubSurface::~TestSubSurface() {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface_);
  if (mock_surface)
    mock_surface->set_sub_surface(nullptr);
}

void TestSubSurface::SetPosition(int x, int y) {
  position_ = gfx::Point(x, y);
}

}  // namespace wl
