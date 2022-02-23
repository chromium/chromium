// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"

#include "build/build_config.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_screen_position_client.h"

namespace views {

bool DesktopWindowTreeHost::IsMoveLoopSupported() const {
  return true;
}

void DesktopWindowTreeHost::SetBoundsInDIP(const gfx::Rect& bounds) {
#if BUILDFLAG(IS_WIN)
  // The window parameter is intentionally passed as nullptr on Windows because
  // a non-null window parameter causes errors when restoring windows to saved
  // positions in variable-DPI situations. See https://crbug.com/1224715 for
  // details.
  aura::Window* root = nullptr;
#else
  aura::Window* root = AsWindowTreeHost()->window();
#endif
  const gfx::Rect bounds_in_pixels =
      display::Screen::GetScreen()->DIPToScreenRectInWindow(root, bounds);
  AsWindowTreeHost()->SetBoundsInPixels(bounds_in_pixels);
}

void DesktopWindowTreeHost::UpdateWindowShapeIfNeeded(
    const ui::PaintContext& context) {}

std::unique_ptr<aura::client::ScreenPositionClient>
DesktopWindowTreeHost::CreateScreenPositionClient() {
  return std::make_unique<DesktopScreenPositionClient>(
      AsWindowTreeHost()->window());
}

DesktopNativeCursorManager*
DesktopWindowTreeHost::GetSingletonDesktopNativeCursorManager() {
  return new DesktopNativeCursorManager();
}

}  // namespace views
