// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window_delegate.h"

#include "base/notreached.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

PlatformWindowDelegate::BoundsChange::BoundsChange() = default;

PlatformWindowDelegate::BoundsChange::BoundsChange(const gfx::Rect& bounds)
    : bounds(bounds) {}

PlatformWindowDelegate::BoundsChange::~BoundsChange() = default;

PlatformWindowDelegate::PlatformWindowDelegate() = default;

PlatformWindowDelegate::~PlatformWindowDelegate() = default;

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

absl::optional<OwnedWindowAnchor>
PlatformWindowDelegate::GetOwnedWindowAnchorAndRectInPx() {
  return absl::nullopt;
}

void PlatformWindowDelegate::SetFrameRateThrottleEnabled(bool enabled) {}

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
