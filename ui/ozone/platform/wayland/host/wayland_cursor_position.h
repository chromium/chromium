// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_POSITION_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_POSITION_H_

#include <ostream>

#include "ui/gfx/geometry/point.h"

namespace ui {

// Stores last known cursor pointer position relative to 0,0 origin
// and returns it on request.
class WaylandCursorPosition {
 public:
  WaylandCursorPosition();

  WaylandCursorPosition(const WaylandCursorPosition&) = delete;
  WaylandCursorPosition& operator=(const WaylandCursorPosition&) = delete;

  ~WaylandCursorPosition();

  void OnCursorPositionChanged(const gfx::Point& cursor_position);

  // Returns last known cursor position relative to 0,0 origin.
  // It is unknown what surface receives that cursor position.
  gfx::Point GetCursorSurfacePoint() const;

  void DumpState(std::ostream& out) const;

 private:
  gfx::Point cursor_surface_point_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_POSITION_H_
