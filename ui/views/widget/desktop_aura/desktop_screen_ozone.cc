// Copyright 2018 The Chromium Authors
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

std::unique_ptr<display::Screen> CreateDesktopScreen() {
  return std::make_unique<DesktopScreenOzone>();
}

}  // namespace views
