// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_wayland_platform_window_delegate.h"

#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

gfx::Rect MockWaylandPlatformWindowDelegate::ConvertRectToPixels(
    const gfx::Rect& rect_in_dp) const {
  float scale = wayland_window_ ? wayland_window_->window_scale() : 1.0f;
  return gfx::ScaleToEnclosingRect(rect_in_dp, scale);
}

gfx::Rect MockWaylandPlatformWindowDelegate::ConvertRectToDIP(
    const gfx::Rect& rect_in_pixels) const {
  float scale = wayland_window_ ? wayland_window_->window_scale() : 1.0f;
  return gfx::ScaleToEnclosedRect(rect_in_pixels, 1.0f / scale);
}

std::unique_ptr<WaylandWindow>
MockWaylandPlatformWindowDelegate::CreateWaylandWindow(
    WaylandConnection* connection,
    PlatformWindowInitProperties properties,
    bool update_visual_size_immediately,
    bool apply_pending_state_on_update_visual_size) {
  auto window = WaylandWindow::Create(
      this, connection, std::move(properties), update_visual_size_immediately,
      apply_pending_state_on_update_visual_size);
  wayland_window_ = window.get();
  return window;
}

int64_t MockWaylandPlatformWindowDelegate::InsertSequencePoint() {
  return viz_seq_++;
}

}  // namespace ui
