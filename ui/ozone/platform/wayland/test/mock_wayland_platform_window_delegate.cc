// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_wayland_platform_window_delegate.h"

#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

MockWaylandPlatformWindowDelegate::MockWaylandPlatformWindowDelegate() =
    default;
MockWaylandPlatformWindowDelegate::~MockWaylandPlatformWindowDelegate() =
    default;

gfx::Rect MockWaylandPlatformWindowDelegate::ConvertRectToPixels(
    const gfx::Rect& rect_in_dp) const {
  float scale =
      wayland_window_ ? wayland_window_->applied_state().window_scale : 1.0f;
  return gfx::ScaleToEnclosingRectIgnoringError(rect_in_dp, scale);
}

gfx::Rect MockWaylandPlatformWindowDelegate::ConvertRectToDIP(
    const gfx::Rect& rect_in_pixels) const {
  float scale =
      wayland_window_ ? wayland_window_->applied_state().window_scale : 1.0f;
  return gfx::ScaleToEnclosedRect(rect_in_pixels, 1.0f / scale);
}

std::unique_ptr<WaylandWindow>
MockWaylandPlatformWindowDelegate::CreateWaylandWindow(
    WaylandConnection* connection,
    PlatformWindowInitProperties properties) {
  auto window = WaylandWindow::Create(this, connection, std::move(properties));
  wayland_window_ = window.get();
  return window;
}

int64_t MockWaylandPlatformWindowDelegate::OnStateUpdate(
    const PlatformWindowDelegate::State& old,
    const PlatformWindowDelegate::State& latest) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (old.fullscreen_type != latest.fullscreen_type) {
    OnFullscreenTypeChanged(old.fullscreen_type, latest.fullscreen_type);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  if (old.window_state != latest.window_state) {
    OnWindowStateChanged(old.window_state, latest.window_state);
  }

  if (old.bounds_dip != latest.bounds_dip || old.size_px != latest.size_px ||
      old.window_scale != latest.window_scale) {
    bool origin_changed = old.bounds_dip.origin() != latest.bounds_dip.origin();
    OnBoundsChanged({origin_changed});
  }

  if (old.occlusion_state != latest.occlusion_state) {
    OnOcclusionStateChanged(latest.occlusion_state);
  }

  if (!on_state_update_callback_.is_null()) {
    on_state_update_callback_.Run();
  }

  if (!latest.WillProduceFrameOnUpdateFrom(old)) {
    return -1;
  }

  return ++viz_seq_;
}

}  // namespace ui
