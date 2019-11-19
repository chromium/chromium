// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "ui/base/x/x11_display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/views/linux_ui/device_scale_factor_observer.h"
#include "ui/views/views_export.h"

namespace views {
class DesktopScreenX11Test;

namespace test {
class DesktopScreenX11TestApi;
}

// Screen implementation that talks to XRandR
class VIEWS_EXPORT DesktopScreenX11 : public display::Screen,
                                      public ui::PlatformEventDispatcher,
                                      public ui::XDisplayManager::Delegate,
                                      public views::DeviceScaleFactorObserver {
 public:
  DesktopScreenX11();
  ~DesktopScreenX11() override;

  // Fetches display list using XRandR. Must be called explicitly as actual
  // fetching might not be desirable in some scenarios (e.g: unit tests)
  void Init();

  // Overridden from display::Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeView window) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  display::Display GetPrimaryDisplay() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  // views::DeviceScaleFactorObserver:
  void OnDeviceScaleFactorChanged() override;

  static void UpdateDeviceScaleFactorForTest();

 private:
  friend class DesktopScreenX11Test;
  friend class test::DesktopScreenX11TestApi;

  // ui::XDisplayManager::Delegate
  void OnXDisplayListUpdated() override;
  float GetXDisplayScaleFactor() override;

  std::unique_ptr<ui::XDisplayManager> x11_display_manager_;

  DISALLOW_COPY_AND_ASSIGN(DesktopScreenX11);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_
