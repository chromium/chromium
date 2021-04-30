// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_ozone.h"

#include <memory>

#include "build/build_config.h"
#include "ui/aura/screen_ozone.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace views {

DesktopScreenOzone::DesktopScreenOzone() = default;

DesktopScreenOzone::~DesktopScreenOzone() = default;

gfx::NativeWindow DesktopScreenOzone::GetNativeWindowFromAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  if (!widget)
    return nullptr;
  return views::DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
      widget);
}

// To avoid multiple definitions when use_x11 && use_ozone is true, disable this
// factory method for OS_LINUX as Linux has a factory method that decides what
// screen to use based on IsUsingOzonePlatform feature flag.
#if !defined(OS_LINUX) && !defined(OS_CHROMEOS)
std::unique_ptr<display::Screen> CreateDesktopScreen() {
  return std::make_unique<aura::ScreenOzone>();
}
#endif

}  // namespace views
