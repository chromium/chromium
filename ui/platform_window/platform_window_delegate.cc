// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window_delegate.h"

#include "base/notreached.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

PlatformWindowDelegate::PlatformWindowDelegate() = default;

PlatformWindowDelegate::~PlatformWindowDelegate() = default;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
void PlatformWindowDelegate::OnWindowTiledStateChanged(
    WindowTiledEdges new_tiled_edges) {}
#endif

absl::optional<gfx::Size> PlatformWindowDelegate::GetMinimumSizeForWindow() {
  return absl::nullopt;
}

absl::optional<gfx::Size> PlatformWindowDelegate::GetMaximumSizeForWindow() {
  return absl::nullopt;
}

SkPath PlatformWindowDelegate::GetWindowMaskForWindowShapeInPixels() {
  return SkPath();
}

void PlatformWindowDelegate::OnSurfaceFrameLockingChanged(bool lock) {}

absl::optional<MenuType> PlatformWindowDelegate::GetMenuType() {
  return absl::nullopt;
}

void PlatformWindowDelegate::OnOcclusionStateChanged(
    PlatformWindowOcclusionState occlusion_state) {}

int64_t PlatformWindowDelegate::InsertSequencePoint() {
  NOTREACHED();
  return -1;
}

absl::optional<OwnedWindowAnchor>
PlatformWindowDelegate::GetOwnedWindowAnchorAndRectInDIP() {
  return absl::nullopt;
}

void PlatformWindowDelegate::SetFrameRateThrottleEnabled(bool enabled) {}

void PlatformWindowDelegate::OnTooltipShownOnServer(const std::u16string& text,
                                                    const gfx::Rect& bounds) {}

void PlatformWindowDelegate::OnTooltipHiddenOnServer() {}

gfx::Rect PlatformWindowDelegate::ConvertRectToPixels(
    const gfx::Rect& rect_in_dip) const {
  return rect_in_dip;
}

gfx::Rect PlatformWindowDelegate::ConvertRectToDIP(
    const gfx::Rect& rect_in_pixels) const {
  return rect_in_pixels;
}

gfx::PointF PlatformWindowDelegate::ConvertScreenPointToLocalDIP(
    const gfx::Point& screen_in_pixels) const {
  return gfx::PointF(screen_in_pixels);
}

}  // namespace ui
