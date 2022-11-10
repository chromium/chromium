// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_POSITIONER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_POSITIONER_H_

#include <utility>

#include <xdg-shell-server-protocol.h>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct xdg_positioner_interface kTestXdgPositionerImpl;
extern const struct zxdg_positioner_v6_interface kTestZxdgPositionerV6Impl;

// A simple positioner object that provides a collection of rules of a child
// surface relative to a parent surface.
class TestPositioner : public ServerObject {
 public:
  struct PopupPosition {
    gfx::Rect anchor_rect;
    gfx::Size size;
    uint32_t anchor = 0;
    uint32_t gravity = 0;
    uint32_t constraint_adjustment = 0;
  };

  explicit TestPositioner(wl_resource* resource);

  TestPositioner(const TestPositioner&) = delete;
  TestPositioner& operator=(const TestPositioner&) = delete;

  ~TestPositioner() override;

  PopupPosition position() { return std::move(position_); }
  void set_size(gfx::Size size) { position_.size = size; }
  void set_anchor_rect(gfx::Rect anchor_rect) {
    position_.anchor_rect = anchor_rect;
  }
  void set_anchor(uint32_t anchor) { position_.anchor = anchor; }
  void set_gravity(uint32_t gravity) { position_.gravity = gravity; }
  void set_constraint_adjustment(uint32_t constraint_adjustment) {
    position_.constraint_adjustment = constraint_adjustment;
  }

 private:
  PopupPosition position_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_POSITIONER_H_
