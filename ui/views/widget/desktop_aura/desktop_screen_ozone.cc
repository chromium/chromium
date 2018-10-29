// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_ozone.h"

#include "ui/aura/screen_ozone.h"
#include "ui/display/display.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"

namespace views {

DesktopScreenOzone::DesktopScreenOzone()
    : delegate_(
          ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate()) {
  delegate_->AddObserver(this);
  delegate_->Initialize();
}

DesktopScreenOzone::~DesktopScreenOzone() = default;

void DesktopScreenOzone::OnHostDisplaysReady(
    const std::vector<display::DisplaySnapshot*>& displays) {
  DCHECK(!displays.empty());
  // TODO(msisov): Add support for multiple displays.
  display::DisplaySnapshot* display_snapshot = displays.front();
  DCHECK(display_snapshot);

  float device_scale_factor = 1.f;
  if (display::Display::HasForceDeviceScaleFactor())
    device_scale_factor = display::Display::GetForcedDeviceScaleFactor();

  gfx::Size scaled_size = gfx::ConvertSizeToDIP(
      device_scale_factor, display_snapshot->current_mode()->size());

  display::Display display(display_snapshot->display_id());
  display.set_bounds(gfx::Rect(scaled_size));
  display.set_work_area(display.bounds());
  display.set_device_scale_factor(device_scale_factor);

  ProcessDisplayChanged(display, true /* is_primary */);
}

void DesktopScreenOzone::OnConfigurationChanged() {
  delegate_->GetDisplays(base::BindOnce(
      &DesktopScreenOzone::OnHostDisplaysReady, base::Unretained(this)));
}

void DesktopScreenOzone::OnDisplaySnapshotsInvalidated() {}

//////////////////////////////////////////////////////////////////////////////

display::Screen* CreateDesktopScreen() {
  auto platform_screen = ui::OzonePlatform::GetInstance()->CreateScreen();
  if (!platform_screen) {
    // TODO: At the moment, only the Ozone/Headless uses this patch. Fix it:
    // https://crbug.com/891613
    LOG(ERROR) << "PlatformScreen is not implemented for this ozone platform. "
                  "Falling back to old DesktopScreenOzone implementation. See "
                  "https://crbug.com/872339 for details";
    return new DesktopScreenOzone;
  }
  return new aura::ScreenOzone(std::move(platform_screen));
}

}  // namespace views
