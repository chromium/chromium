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

gfx::Point WaylandCursorPosition::GetCursorSurfacePoint() const {
  return cursor_surface_point_;
}

void WaylandCursorPosition::DumpState(std::ostream& out) const {
  out << "WaylandCursorPositoin: cursor_surface_point:"
      << cursor_surface_point_.ToString();
}

}  // namespace ui
