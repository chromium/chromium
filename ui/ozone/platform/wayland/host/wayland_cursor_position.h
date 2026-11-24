// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_POSITION_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_POSITION_H_

#include <optional>
#include <ostream>

#include "ui/gfx/geometry/point.h"

namespace ui {

// Stores last known cursor pointer position and touch position relative to
// 0,0 origin and returns it on request. The touch position takes precedence
// while a touch point is down, so touch does not permanently move the cursor.
class WaylandCursorPosition {
 public:
  WaylandCursorPosition();

  WaylandCursorPosition(const WaylandCursorPosition&) = delete;
  WaylandCursorPosition& operator=(const WaylandCursorPosition&) = delete;

  ~WaylandCursorPosition();

  void OnCursorPositionChanged(const gfx::Point& cursor_position);

  // Called for touch events while a touch point is down.
  void OnTouchPositionChanged(const gfx::Point& touch_position);

  // Called once no touch point is down anymore.
  void OnTouchReleased();

  // Returns the last known touch position while a touch point is down, and the
  // last known cursor position relative to 0,0 origin otherwise.
  // It is unknown what surface receives that position.
  gfx::Point GetCursorSurfacePoint() const;

  void DumpState(std::ostream& out) const;

 private:
  gfx::Point cursor_surface_point_;
  std::optional<gfx::Point> touch_surface_point_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_POSITION_H_
