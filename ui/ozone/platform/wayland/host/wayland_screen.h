// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SCREEN_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SCREEN_H_

#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "ui/display/display_list.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/public/platform_screen.h"

namespace gfx {
class Rect;
}

namespace ui {

class WaylandConnection;

// A PlatformScreen implementation for Wayland.
class WaylandScreen : public PlatformScreen {
 public:
  explicit WaylandScreen(WaylandConnection* connection);
  WaylandScreen(const WaylandScreen&) = delete;
  WaylandScreen& operator=(const WaylandScreen&) = delete;
  ~WaylandScreen() override;

  void OnOutputAddedOrUpdated(uint32_t output_id,
                              const gfx::Rect& bounds,
                              int32_t output_scale);
  void OnOutputRemoved(uint32_t output_id);

  base::WeakPtr<WaylandScreen> GetWeakPtr();

  // PlatformScreen overrides:
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const override;
  gfx::Point GetCursorScreenPoint() const override;
  gfx::AcceleratedWidget GetAcceleratedWidgetAtScreenPoint(
      const gfx::Point& point) const override;
  gfx::AcceleratedWidget GetLocalProcessWidgetAtPoint(
      const gfx::Point& point,
      const std::set<gfx::AcceleratedWidget>& ignore) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

 private:
  void AddOrUpdateDisplay(uint32_t output_id,
                          const gfx::Rect& bounds,
                          int32_t scale);

  WaylandConnection* connection_ = nullptr;

  display::DisplayList display_list_;

  base::ObserverList<display::DisplayObserver> observers_;

  base::Optional<gfx::BufferFormat> image_format_alpha_;
  base::Optional<gfx::BufferFormat> image_format_no_alpha_;

  base::WeakPtrFactory<WaylandScreen> weak_factory_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SCREEN_H_
