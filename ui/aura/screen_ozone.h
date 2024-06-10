// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCREEN_OZONE_H_
#define UI_AURA_SCREEN_OZONE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "ui/aura/aura_export.h"
#include "ui/display/screen.h"
#include "ui/ozone/public/platform_screen.h"

namespace aura {

// display::Screen implementation on top of ui::PlatformScreen provided by
// Ozone.
class AURA_EXPORT ScreenOzone : public display::Screen {
 public:
  ScreenOzone();

  ScreenOzone(const ScreenOzone&) = delete;
  ScreenOzone& operator=(const ScreenOzone&) = delete;

  ~ScreenOzone() override;

  // display::Screen interface.
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override;
  int GetNumDisplays() const override;
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;
  display::Display GetDisplayNearestView(gfx::NativeView view) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  display::Display GetPrimaryDisplay() const override;
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  std::unique_ptr<display::Screen::ScreenSaverSuspender> SuspendScreenSaver()
      override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  bool IsScreenSaverActive() const override;
  base::TimeDelta CalculateIdleTime() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;
  std::string GetCurrentWorkspace() override;
  base::Value::List GetGpuExtraInfo(
      const gfx::GpuExtraInfo& gpu_extra_info) override;
  std::optional<float> GetPreferredScaleFactorForWindow(
      gfx::NativeWindow window) const override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  display::TabletState GetTabletState() const override;
  void OverrideTabletStateForTesting(
      display::TabletState tablet_state) override;
#endif

  // Returns the NativeWindow associated with the AcceleratedWidget.
  virtual gfx::NativeWindow GetNativeWindowFromAcceleratedWidget(
      gfx::AcceleratedWidget widget) const;

  static bool IsOzoneInitialized();

 protected:
  ui::PlatformScreen* platform_screen() { return platform_screen_.get(); }

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  class ScreenSaverSuspenderOzone
      : public display::Screen::ScreenSaverSuspender {
   public:
    explicit ScreenSaverSuspenderOzone(
        std::unique_ptr<ui::PlatformScreen::PlatformScreenSaverSuspender>
            suspender);

    ScreenSaverSuspenderOzone(const ScreenSaverSuspenderOzone&) = delete;
    ScreenSaverSuspenderOzone& operator=(const ScreenSaverSuspenderOzone&) =
        delete;

    ~ScreenSaverSuspenderOzone() override;

   private:
    std::unique_ptr<ui::PlatformScreen::PlatformScreenSaverSuspender>
        suspender_;
  };
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)

  gfx::AcceleratedWidget GetAcceleratedWidgetForWindow(
      aura::Window* window) const;

  std::unique_ptr<ui::PlatformScreen> platform_screen_;
};

}  // namespace aura

#endif  // UI_AURA_SCREEN_OZONE_H_
