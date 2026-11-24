// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"

#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandCursorPosition::WaylandCursorPosition() = default;

WaylandCursorPosition::~WaylandCursorPosition() = default;

void WaylandCursorPosition::OnCursorPositionChanged(
    const gfx::Point& cursor_position) {
  cursor_surface_point_ = cursor_position;
}

void WaylandCursorPosition::OnTouchPositionChanged(
    const gfx::Point& touch_position) {
  touch_surface_point_ = touch_position;
}

void WaylandCursorPosition::OnTouchReleased() {
  touch_surface_point_.reset();
}

gfx::Point WaylandCursorPosition::GetCursorSurfacePoint() const {
  return touch_surface_point_.value_or(cursor_surface_point_);
}

void WaylandCursorPosition::DumpState(std::ostream& out) const {
  out << "WaylandCursorPosition: cursor_surface_point:"
      << cursor_surface_point_.ToString() << " touch_surface_point:"
      << (touch_surface_point_ ? touch_surface_point_->ToString() : "none");
}

}  // namespace ui
