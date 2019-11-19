// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_positioner.h"

#include <wayland-server-core.h>

namespace wl {

namespace {

void SetSize(struct wl_client* wl_client,
             struct wl_resource* resource,
             int32_t width,
             int32_t height) {
  if (width < 1 || height < 1) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "width and height must be positive and non-zero");
    return;
  }

  GetUserDataAs<TestPositioner>(resource)->set_size(gfx::Size(width, height));
}

void SetAnchorRect(struct wl_client* client,
                   struct wl_resource* resource,
                   int32_t x,
                   int32_t y,
                   int32_t width,
                   int32_t height) {
  if (width < 1 || height < 1) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "width and height must be positive and non-zero");
    return;
  }

  GetUserDataAs<TestPositioner>(resource)->set_anchor_rect(
      gfx::Rect(x, y, width, height));
}

void SetAnchor(struct wl_client* wl_client,
               struct wl_resource* resource,
               uint32_t anchor) {
  if (((anchor & ZXDG_POSITIONER_V6_ANCHOR_LEFT) &&
       (anchor & ZXDG_POSITIONER_V6_ANCHOR_RIGHT)) ||
      ((anchor & ZXDG_POSITIONER_V6_ANCHOR_TOP) &&
       (anchor & ZXDG_POSITIONER_V6_ANCHOR_BOTTOM))) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "same-axis values are not allowed");
    return;
  }

  GetUserDataAs<TestPositioner>(resource)->set_anchor(anchor);
}

void SetGravity(struct wl_client* client,
                struct wl_resource* resource,
                uint32_t gravity) {
  if (((gravity & ZXDG_POSITIONER_V6_GRAVITY_LEFT) &&
       (gravity & ZXDG_POSITIONER_V6_GRAVITY_RIGHT)) ||
      ((gravity & ZXDG_POSITIONER_V6_GRAVITY_TOP) &&
       (gravity & ZXDG_POSITIONER_V6_GRAVITY_BOTTOM))) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "same-axis values are not allowed");
    return;
  }

  GetUserDataAs<TestPositioner>(resource)->set_gravity(gravity);
}

void SetConstraintAdjustment(struct wl_client* client,
                             struct wl_resource* resource,
                             uint32_t constraint_adjustment) {
  GetUserDataAs<TestPositioner>(resource)->set_constraint_adjustment(
      constraint_adjustment);
}

}  // namespace

const struct zxdg_positioner_v6_interface kTestZxdgPositionerV6Impl = {
    &DestroyResource,          // destroy
    &SetSize,                  // set_size
    &SetAnchorRect,            // set_anchor_rect
    &SetAnchor,                // set_anchor
    &SetGravity,               // set_gravity
    &SetConstraintAdjustment,  // set_constraint_adjustment
    nullptr,                   // set_offset
};

TestPositioner::TestPositioner(wl_resource* resource)
    : ServerObject(resource) {}

TestPositioner::~TestPositioner() {}

}  // namespace wl
