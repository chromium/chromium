// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SCREEN_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SCREEN_H_

#include <optional>
#include <ostream>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "ui/display/display_list.h"
#include "ui/display/display_observer.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/public/platform_screen.h"

#if BUILDFLAG(IS_LINUX)
#include "base/scoped_observation.h"
#include "ui/linux/device_scale_factor_observer.h"
#include "ui/linux/linux_ui.h"
#endif

namespace gfx {
class Rect;
}

namespace ui {

class WaylandConnection;

#if defined(USE_DBUS)
class OrgGnomeMutterIdleMonitor;
#endif

// A PlatformScreen implementation for Wayland.
class WaylandScreen : public PlatformScreen
#if BUILDFLAG(IS_LINUX)
    ,
                      public DeviceScaleFactorObserver
#endif
{
 public:
  explicit WaylandScreen(WaylandConnection* connection);
  WaylandScreen(const WaylandScreen&) = delete;
  WaylandScreen& operator=(const WaylandScreen&) = delete;
  ~WaylandScreen() override;

  void OnOutputAddedOrUpdated(const WaylandOutput::Metrics& metrics);
  void OnOutputRemoved(uint32_t output_id);

  WaylandOutput::Id GetOutputIdForDisplayId(int64_t display_id);
  WaylandOutput* GetWaylandOutputForDisplayId(int64_t display_id);

  // Returns id of the output that matches the bounds in screen coordinates.
  WaylandOutput::Id GetOutputIdMatching(const gfx::Rect& match_rect);

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
  std::unique_ptr<PlatformScreen::PlatformScreenSaverSuspender>
  SuspendScreenSaver() override;
  bool IsScreenSaverActive() const override;
  base::TimeDelta CalculateIdleTime() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;
  base::Value::List GetGpuExtraInfo(
      const gfx::GpuExtraInfo& gpu_extra_info) override;
  std::optional<float> GetPreferredScaleFactorForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnTabletStateChanged(display::TabletState tablet_state) override;
  display::TabletState GetTabletState() const override;
#endif

#if BUILDFLAG(IS_LINUX)
  // DeviceScaleFactorObserver:
  void OnDeviceScaleFactorChanged() override;
#endif

  void DumpState(std::ostream& out) const;

  // True if the internal representations for output objects is consistent for
  // the screen.
  bool VerifyOutputStateConsistentForTesting() const;

 protected:
  // Suspends or un-suspends the platform-specific screensaver, and returns
  // whether the operation was successful. Can be called more than once with the
  // same value for |suspend|, but those states should not stack: the first
  // alternating value should toggle the state of the suspend.
  bool SetScreenSaverSuspended(bool suspend);

 private:
  class WaylandScreenSaverSuspender
      : public PlatformScreen::PlatformScreenSaverSuspender {
   public:
    WaylandScreenSaverSuspender(const WaylandScreenSaverSuspender&) = delete;
    WaylandScreenSaverSuspender& operator=(const WaylandScreenSaverSuspender&) =
        delete;

    ~WaylandScreenSaverSuspender() override;

    static std::unique_ptr<WaylandScreenSaverSuspender> Create(
        WaylandScreen& screen);

   private:
    explicit WaylandScreenSaverSuspender(WaylandScreen& screen);

    base::WeakPtr<WaylandScreen> screen_;
    bool is_suspending_ = false;
  };

  void AddOrUpdateDisplay(const WaylandOutput::Metrics& metrics);
  // Dangling on DemoIntegrationTest.NewTab on lacros-amd64-generic-rel-gtest
  raw_ptr<WaylandConnection, DanglingUntriaged> connection_ = nullptr;

  base::flat_map<WaylandOutput::Id, int64_t> display_id_map_;
  display::DisplayList display_list_;

  std::optional<gfx::BufferFormat> image_format_alpha_;
  std::optional<gfx::BufferFormat> image_format_no_alpha_;
  std::optional<gfx::BufferFormat> image_format_hdr_;

#if defined(USE_DBUS)
  mutable std::unique_ptr<OrgGnomeMutterIdleMonitor>
      org_gnome_mutter_idle_monitor_;
#endif

  wl::Object<zwp_idle_inhibitor_v1> idle_inhibitor_;
  uint32_t screen_saver_suspension_count_ = 0;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  display::TabletState tablet_state_;
#endif

#if BUILDFLAG(IS_LINUX)
  base::ScopedObservation<ui::LinuxUi, DeviceScaleFactorObserver>
      display_scale_factor_observer_{this};
#endif

  base::WeakPtrFactory<WaylandScreen> weak_factory_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SCREEN_H_
