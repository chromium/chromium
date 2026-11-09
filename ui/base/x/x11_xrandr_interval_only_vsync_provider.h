// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_XRANDR_INTERVAL_ONLY_VSYNC_PROVIDER_H_
#define UI_BASE_X_X11_XRANDR_INTERVAL_ONLY_VSYNC_PROVIDER_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {
class GeometryCache;
}

namespace ui {

// Provides the refresh interval of an XRandR output.  If `window` is not
// `x11::Window::None`, the output that contains the largest part of `window`
// is used.  Otherwise, or until the bounds of `window` are known, the primary
// output is used.
class COMPONENT_EXPORT(UI_BASE_X) XrandrIntervalOnlyVSyncProvider
    : public gfx::VSyncProvider {
 public:
  explicit XrandrIntervalOnlyVSyncProvider(
      x11::Window window = x11::Window::None);
  XrandrIntervalOnlyVSyncProvider(const XrandrIntervalOnlyVSyncProvider&) =
      delete;
  XrandrIntervalOnlyVSyncProvider& operator=(
      const XrandrIntervalOnlyVSyncProvider&) = delete;
  ~XrandrIntervalOnlyVSyncProvider() override;

  // gfx::VSyncProvider:
  void GetVSyncParameters(UpdateVSyncCallback callback) override;
  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override;
  bool SupportGetVSyncParametersIfAvailable() const override;
  bool IsHWClock() const override;

 private:
  void OnWindowBoundsChanged(const std::optional<gfx::Rect>& old_bounds,
                             const gfx::Rect& new_bounds);

  // Tracks the bounds of the window in root window coordinates.
  std::unique_ptr<x11::GeometryCache> geometry_cache_;

  // The bounds of the window in root window coordinates, once known.
  std::optional<gfx::Rect> window_bounds_;

  base::WeakPtrFactory<XrandrIntervalOnlyVSyncProvider> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_X_X11_XRANDR_INTERVAL_ONLY_VSYNC_PROVIDER_H_
