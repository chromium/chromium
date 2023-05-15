// Copyright 2019 The Chromium Authors
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

void DesktopWindowTreeHost::UpdateWindowShapeIfNeeded(
    const ui::PaintContext& context) {}

void DesktopWindowTreeHost::PaintAsActiveChanged() {}

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
