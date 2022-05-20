// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SCREEN_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SCREEN_H_

#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display_list.h"
#include "ui/display/display_observer.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/public/platform_screen.h"

namespace gfx {
class Rect;
}

namespace ui {

class WaylandConnection;

#if defined(USE_DBUS)
class OrgGnomeMutterIdleMonitor;
#endif

// A PlatformScreen implementation for Wayland.
class WaylandScreen : public PlatformScreen {
 public:
  explicit WaylandScreen(WaylandConnection* connection);
  WaylandScreen(const WaylandScreen&) = delete;
  WaylandScreen& operator=(const WaylandScreen&) = delete;
  ~WaylandScreen() override;

  void OnOutputAddedOrUpdated(uint32_t output_id,
                              const gfx::Rect& bounds,
                              float output_scale,
                              int32_t output_transform);
  void OnOutputRemoved(uint32_t output_id);

  void OnTabletStateChanged(display::TabletState tablet_state);

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
  bool SetScreenSaverSuspended(bool suspend) override;
  bool IsScreenSaverActive() const override;
  base::TimeDelta CalculateIdleTime() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;
  std::vector<base::Value> GetGpuExtraInfo(
      const gfx::GpuExtraInfo& gpu_extra_info) override;

 private:
  void AddOrUpdateDisplay(uint32_t output_id,
                          const gfx::Rect& bounds,
                          float scale,
                          int32_t transform);

  WaylandConnection* connection_ = nullptr;

  display::DisplayList display_list_;

  base::ObserverList<display::DisplayObserver> observers_;

  absl::optional<gfx::BufferFormat> image_format_alpha_;
  absl::optional<gfx::BufferFormat> image_format_no_alpha_;

#if defined(USE_DBUS)
  mutable std::unique_ptr<OrgGnomeMutterIdleMonitor>
      org_gnome_mutter_idle_monitor_;
#endif

  wl::Object<zwp_idle_inhibitor_v1> idle_inhibitor_;

  base::WeakPtrFactory<WaylandScreen> weak_factory_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SCREEN_H_
