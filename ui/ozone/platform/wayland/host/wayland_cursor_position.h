// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_POSITION_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_POSITION_H_

#include "base/macros.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

// Stores last known cursor pointer position in regards to top-level windows'
// coordinates and returns it on request.
class WaylandCursorPosition {
 public:
  WaylandCursorPosition();
  ~WaylandCursorPosition();

  void OnCursorPositionChanged(const gfx::Point& cursor_position);

  // Returns last known cursor position in regards to top-level surface local
  // coordinates. It is unknown what surface receives that cursor position.
  gfx::Point GetCursorSurfacePoint() const;

 private:
  gfx::Point cursor_surface_point_;

  DISALLOW_COPY_AND_ASSIGN(WaylandCursorPosition);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_POSITION_H_
