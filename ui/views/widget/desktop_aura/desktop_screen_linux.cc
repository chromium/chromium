// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen.h"

#include "base/notreached.h"

#if defined(USE_X11)
#include "ui/views/widget/desktop_aura/desktop_screen_x11.h"
#endif

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/platform_screen.h"
#include "ui/views/linux_ui/device_scale_factor_observer.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/widget/desktop_aura/desktop_screen_ozone.h"
#endif

namespace views {

#if defined(USE_OZONE)
// Listens to device scale factor changes that can be provided via "external" to
// Ozone sources such as toolkits, etc, and provides Ozone with new values.
class DesktopScreenOzoneLinux : public DesktopScreenOzone,
                                public DeviceScaleFactorObserver {
 public:
  DesktopScreenOzoneLinux() {
    auto* linux_ui = LinuxUI::instance();
    if (linux_ui) {
      display_scale_factor_observer_.Observe(linux_ui);

      // Send current scale factor as starting to observe doesn't actually
      // result in getting a OnDeviceScaleFactorChanged call.
      SetDeviceScaleFactorToPlatformScreen(linux_ui->GetDeviceScaleFactor());
    }
  }
  ~DesktopScreenOzoneLinux() override = default;

 private:
  // DeviceScaleFactorObserver:
  void OnDeviceScaleFactorChanged() override {
    SetDeviceScaleFactorToPlatformScreen(
        LinuxUI::instance()->GetDeviceScaleFactor());
  }

  void SetDeviceScaleFactorToPlatformScreen(float scale_factor) {
    platform_screen()->SetDeviceScaleFactor(scale_factor);
  }

  base::ScopedObservation<LinuxUI,
                          DeviceScaleFactorObserver,
                          &LinuxUI::AddDeviceScaleFactorObserver,
                          &LinuxUI::RemoveDeviceScaleFactorObserver>
      display_scale_factor_observer_{this};
};

#endif

std::unique_ptr<display::Screen> CreateDesktopScreen() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    return std::make_unique<DesktopScreenOzoneLinux>();
#endif
#if defined(USE_X11)
  auto screen = std::make_unique<DesktopScreenX11>();
  screen->Init();
  return screen;
#else
  NOTREACHED();
  return nullptr;
#endif
}

}  // namespace views
