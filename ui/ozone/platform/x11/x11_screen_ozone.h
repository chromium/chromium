// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_

#include <memory>
#include <utility>
#include <vector>

#include "ui/base/x/x11_display_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/event.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

class X11WindowManager;

// A PlatformScreen implementation for X11.
class X11ScreenOzone : public PlatformScreen,
                       public x11::EventObserver,
                       public XDisplayManager::Delegate {
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
  bool SetScreenSaverSuspended(bool suspend) override;
  bool IsScreenSaverActive() const override;
  base::TimeDelta CalculateIdleTime() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;
  std::string GetCurrentWorkspace() override;
  std::vector<base::Value> GetGpuExtraInfo(
      const gfx::GpuExtraInfo& gpu_extra_info) override;
  void SetDeviceScaleFactor(float scale) override;

  // Overridden from x11::EventObserver:
  void OnEvent(const x11::Event& event) override;

 private:
  friend class X11ScreenOzoneTest;

  // Overridden from ui::XDisplayManager::Delegate:
  void OnXDisplayListUpdated() override;
  float GetXDisplayScaleFactor() const override;

  gfx::Point GetCursorLocation() const;

  x11::Connection* const connection_;
  X11WindowManager* const window_manager_;
  std::unique_ptr<ui::XDisplayManager> x11_display_manager_;

  // Scale value that DesktopScreenOzoneLinux sets by listening to
  // DeviceScaleFactorObserver.
  float device_scale_factor_ = 1.0f;

  // Indicates that |this| is initialized.
  bool initialized_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_
