// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_win.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

namespace views {

DesktopScreenWin::DesktopScreenWin() = default;

DesktopScreenWin::~DesktopScreenWin() {
  display::Screen::SetScreenInstance(old_screen_);
}

HWND DesktopScreenWin::GetHWNDFromNativeWindow(gfx::NativeWindow window) const {
  aura::WindowTreeHost* host = window->GetHost();
  return host ? host->GetAcceleratedWidget() : nullptr;
}

gfx::NativeWindow DesktopScreenWin::GetNativeWindowFromHWND(HWND hwnd) const {
  return ::IsWindow(hwnd)
             ? DesktopWindowTreeHostWin::GetContentWindowForHWND(hwnd)
             : gfx::kNullNativeWindow;
}

bool DesktopScreenWin::IsNativeWindowOccluded(gfx::NativeWindow window) const {
  return window->GetHost()->GetNativeWindowOcclusionState() ==
         aura::Window::OcclusionState::OCCLUDED;
}

////////////////////////////////////////////////////////////////////////////////

display::Screen* CreateDesktopScreen() {
  return new DesktopScreenWin;
}

}  // namespace views
