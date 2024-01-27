// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/x/x11_display_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/event.h"
#include "ui/ozone/public/platform_screen.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/device_scale_factor_observer.h"
#include "ui/linux/linux_ui.h"
#endif

namespace ui {

class X11WindowManager;

// A PlatformScreen implementation for X11.
class X11ScreenOzone : public PlatformScreen,
                       public x11::EventObserver,
                       public XDisplayManager::Delegate
#if BUILDFLAG(IS_LINUX)
    ,
                       public DeviceScaleFactorObserver
#endif
{
 public:
  X11ScreenOzone();

  X11ScreenOzone(const X11ScreenOzone&) = delete;
  X11ScreenOzone& operator=(const X11ScreenOzone&) = delete;

  ~X11ScreenOzone() override;

  // Fetch display list through Xlib/XRandR
  void Init();

  // Overridden from ui::PlatformScreen:
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const override;
  gfx::Point GetCursorScreenPoint() const override;
  bool IsAcceleratedWidgetUnderCursor(
      gfx::AcceleratedWidget widget) const override;
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
  std::string GetCurrentWorkspace() override;
  base::Value::List GetGpuExtraInfo(
      const gfx::GpuExtraInfo& gpu_extra_info) override;

  // Overridden from x11::EventObserver:
  void OnEvent(const x11::Event& event) override;

 private:
  friend class X11ScreenOzoneTest;

  class X11ScreenSaverSuspender
      : public PlatformScreen::PlatformScreenSaverSuspender {
   public:
    X11ScreenSaverSuspender(const X11ScreenSaverSuspender&) = delete;
    X11ScreenSaverSuspender& operator=(const X11ScreenSaverSuspender&) = delete;

    ~X11ScreenSaverSuspender() override;

    static std::unique_ptr<X11ScreenSaverSuspender> Create();

   private:
    X11ScreenSaverSuspender();

    bool is_suspending_ = false;
  };

  // ui::XDisplayManager::Delegate:
  void OnXDisplayListUpdated() override;

#if BUILDFLAG(IS_LINUX)
  // DeviceScaleFactorObserver:
  void OnDeviceScaleFactorChanged() override;
#endif

  const raw_ptr<x11::Connection> connection_;
  const raw_ptr<X11WindowManager> window_manager_;
  std::unique_ptr<ui::XDisplayManager> x11_display_manager_;

  // Indicates that |this| is initialized.
  bool initialized_ = false;

#if BUILDFLAG(IS_LINUX)
  base::ScopedObservation<ui::LinuxUi, DeviceScaleFactorObserver>
      display_scale_factor_observer_{this};
#endif
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_
