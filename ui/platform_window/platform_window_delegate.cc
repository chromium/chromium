// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window_delegate.h"

#include <sstream>

#include "base/notreached.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

bool IsPlatformWindowStateFullscreen(PlatformWindowState state) {
  return state == PlatformWindowState::kFullScreen ||
         state == PlatformWindowState::kPinnedFullscreen ||
         state == PlatformWindowState::kTrustedPinnedFullscreen;
}

bool PlatformWindowDelegate::State::WillProduceFrameOnUpdateFrom(
    const State& old) const {
  // Changing the bounds origin or fullscreen type will not produce a new frame.
  // Anything else will produce a frame, except for the occlusion state. We do
  // not check that here since there isn't enough information to determine if
  // it will produce a frame, as it depends on whether native occlusion is
  // enabled and if the ui compositor changes visibility.
  // Note: Changing the window state produces a new frame as
  // OnWindowStateChanged will schedule relayout even without the bounds change.
  // On the other hand, the fullscreen type change will not schedule relayout
  // and does not affect producing the frame.
  return old.window_state != window_state ||
         old.bounds_dip.size() != bounds_dip.size() || old.size_px != size_px ||
         old.window_scale != window_scale || old.raster_scale != raster_scale;
}

std::string PlatformWindowDelegate::State::ToString() const {
  std::stringstream result;
  result << "State {";
  result << "window_state = " << static_cast<int>(window_state);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  result << ", fullscreen_type = " << static_cast<int>(fullscreen_type);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  result << ", bounds_dip = " << bounds_dip.ToString();
  result << ", size_px = " << size_px.ToString();
  result << ", window_scale = " << window_scale;
  result << ", raster_scale = " << raster_scale;
  result << ", occlusion_state = " << static_cast<int>(occlusion_state);
  result << "}";
  return result.str();
}

PlatformWindowDelegate::PlatformWindowDelegate() = default;

PlatformWindowDelegate::~PlatformWindowDelegate() = default;

gfx::Insets PlatformWindowDelegate::CalculateInsetsInDIP(
    PlatformWindowState window_state) const {
  return gfx::Insets();
}

#if BUILDFLAG(IS_LINUX)
void PlatformWindowDelegate::OnWindowTiledStateChanged(
    WindowTiledEdges new_tiled_edges) {}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void PlatformWindowDelegate::OnFullscreenTypeChanged(
    PlatformFullscreenType old_type,
    PlatformFullscreenType new_type) {}
#endif

std::optional<gfx::Size> PlatformWindowDelegate::GetMinimumSizeForWindow()
    const {
  return std::nullopt;
}

std::optional<gfx::Size> PlatformWindowDelegate::GetMaximumSizeForWindow()
    const {
  return std::nullopt;
}

bool PlatformWindowDelegate::CanMaximize() const {
  return false;
}

bool PlatformWindowDelegate::CanFullscreen() const {
  return false;
}

SkPath PlatformWindowDelegate::GetWindowMaskForWindowShapeInPixels() {
  return SkPath();
}

void PlatformWindowDelegate::OnSurfaceFrameLockingChanged(bool lock) {}

void PlatformWindowDelegate::OnOcclusionStateChanged(
    PlatformWindowOcclusionState occlusion_state) {}

int64_t PlatformWindowDelegate::OnStateUpdate(const State& old,
                                              const State& latest) {
  NOTREACHED();
  return -1;
}

std::optional<OwnedWindowAnchor>
PlatformWindowDelegate::GetOwnedWindowAnchorAndRectInDIP() {
  return std::nullopt;
}

void PlatformWindowDelegate::SetFrameRateThrottleEnabled(bool enabled) {}

void PlatformWindowDelegate::OnTooltipShownOnServer(const std::u16string& text,
                                                    const gfx::Rect& bounds) {}

bool PlatformWindowDelegate::OnRotateFocus(
    PlatformWindowDelegate::RotateDirection direction,
    bool reset) {
  return false;
}

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

gfx::Insets PlatformWindowDelegate::ConvertInsetsToPixels(
    const gfx::Insets& insets_dip) const {
  return insets_dip;
}

void PlatformWindowDelegate::DisableNativeWindowOcclusion() {}

}  // namespace ui
