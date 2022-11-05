// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_observation.h"
#include "ui/linux/device_scale_factor_observer.h"
#include "ui/linux/linux_ui.h"
#include "ui/ozone/public/platform_screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen_ozone.h"

namespace views {

// Listens to device scale factor changes that can be provided via "external" to
// Ozone sources such as toolkits, etc, and provides Ozone with new values.
class DesktopScreenOzoneLinux : public DesktopScreenOzone,
                                public ui::DeviceScaleFactorObserver {
 public:
  DesktopScreenOzoneLinux() = default;
  ~DesktopScreenOzoneLinux() override = default;

 private:
  // DeviceScaleFactorObserver:
  void OnDeviceScaleFactorChanged() override {
    SetDeviceScaleFactorToPlatformScreen(
        ui::LinuxUi::instance()->GetDeviceScaleFactor());
  }

  void SetDeviceScaleFactorToPlatformScreen(float scale_factor) {
    platform_screen()->SetDeviceScaleFactor(scale_factor);
  }

  // ScreenOzone:
  //
  // Linux/Ozone/X11 must get scale factor from LinuxUI before displays are
  // fetched. X11ScreenOzone fetches lists of displays synchronously only on the
  // first initialization. Later, when it gets a notification about scale factor
  // changes, displays are updated via a task, which results in a first
  // PlatformWindow created with wrong bounds translated from DIP to px. Thus,
  // set the display scale factor as early as possible so that list of displays
  // have correct scale factor from the beginning.
  void OnBeforePlatformScreenInit() override {
    auto* linux_ui = ui::LinuxUi::instance();
    if (linux_ui) {
      display_scale_factor_observer_.Observe(linux_ui);
      // Send current scale factor as starting to observe doesn't actually
      // result in getting a OnDeviceScaleFactorChanged call.
      SetDeviceScaleFactorToPlatformScreen(linux_ui->GetDeviceScaleFactor());
    }
  }

  base::ScopedObservation<ui::LinuxUi, DeviceScaleFactorObserver>
      display_scale_factor_observer_{this};
};

std::unique_ptr<display::Screen> CreateDesktopScreen() {
  auto screen = std::make_unique<DesktopScreenOzoneLinux>();
  screen->Initialize();
  return screen;
}

}  // namespace views
