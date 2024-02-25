// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_win.h"

#include <memory>

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

namespace views {

DesktopScreenWin::DesktopScreenWin() {
  DCHECK(!display::Screen::HasScreen());
  display::Screen::SetScreenInstance(this);
}

DesktopScreenWin::~DesktopScreenWin() {
  display::Screen::SetScreenInstance(nullptr);
}

HWND DesktopScreenWin::GetHWNDFromNativeWindow(gfx::NativeWindow window) const {
  aura::WindowTreeHost* host = window->GetHost();
  return host ? host->GetAcceleratedWidget() : nullptr;
}

gfx::NativeWindow DesktopScreenWin::GetNativeWindowFromHWND(HWND hwnd) const {
  return ::IsWindow(hwnd)
             ? DesktopWindowTreeHostWin::GetContentWindowForHWND(hwnd)
             : gfx::NativeWindow();
}

bool DesktopScreenWin::IsNativeWindowOccluded(gfx::NativeWindow window) const {
  return window->GetHost()->GetNativeWindowOcclusionState() ==
         aura::Window::OcclusionState::OCCLUDED;
}

std::optional<bool> DesktopScreenWin::IsWindowOnCurrentVirtualDesktop(
    gfx::NativeWindow window) const {
  DCHECK(window);
  return window->GetHost()->on_current_workspace();
}

////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<display::Screen> CreateDesktopScreen() {
  return std::make_unique<DesktopScreenWin>();
}

}  // namespace views
