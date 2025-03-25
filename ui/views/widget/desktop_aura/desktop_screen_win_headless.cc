// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_win_headless.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/notimplemented.h"
#include "components/headless/screen_info/headless_screen_info.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#include "ui/views/win/hwnd_util.h"

namespace views {

namespace {

std::vector<headless::HeadlessScreenInfo> GetScreenInfo() {
  std::vector<headless::HeadlessScreenInfo> screen_info;

  const base::CommandLine& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());

  if (command_line.HasSwitch(switches::kScreenInfo)) {
    const std::string switch_value =
        command_line.GetSwitchValueASCII(switches::kScreenInfo);
    auto screen_info_or_error =
        headless::HeadlessScreenInfo::FromString(switch_value);
    CHECK(screen_info_or_error.has_value()) << screen_info_or_error.error();
    screen_info = screen_info_or_error.value();
  } else {
    screen_info.push_back(headless::HeadlessScreenInfo());
  }

  return screen_info;
}

}  // namespace

DesktopScreenWinHeadless::DesktopScreenWinHeadless()
    : display::win::ScreenWinHeadless(GetScreenInfo()) {
  CHECK(!display::Screen::HasScreen());
  display::Screen::SetScreenInstance(this);
}

DesktopScreenWinHeadless::~DesktopScreenWinHeadless() {
  display::Screen::SetScreenInstance(nullptr);
}

HWND DesktopScreenWinHeadless::GetHWNDFromNativeWindow(
    gfx::NativeWindow window) const {
  aura::WindowTreeHost* host = window->GetHost();
  return host ? host->GetAcceleratedWidget() : nullptr;
}

gfx::NativeWindow DesktopScreenWinHeadless::GetNativeWindowFromHWND(
    HWND hwnd) const {
  return ::IsWindow(hwnd)
             ? DesktopWindowTreeHostWin::GetContentWindowForHWND(hwnd)
             : gfx::NativeWindow();
}

bool DesktopScreenWinHeadless::IsNativeWindowOccluded(
    gfx::NativeWindow window) const {
  return window->GetHost()->GetNativeWindowOcclusionState() ==
         aura::Window::OcclusionState::OCCLUDED;
}

std::optional<bool> DesktopScreenWinHeadless::IsWindowOnCurrentVirtualDesktop(
    gfx::NativeWindow window) const {
  CHECK(window);
  return window->GetHost()->on_current_workspace();
}

gfx::Rect DesktopScreenWinHeadless::GetNativeWindowBoundsInScreen(
    gfx::NativeWindow window) const {
  CHECK(window);
  return window->GetBoundsInScreen();
}

gfx::Rect DesktopScreenWinHeadless::GetHeadlessWindowBounds(
    gfx::AcceleratedWidget window) const {
  CHECK(window);
  return views::GetHeadlessWindowBounds(window);
}

std::vector<gfx::NativeWindow>
DesktopScreenWinHeadless::GetNativeWindowsAtScreenPoint(
    const gfx::Point& point) const {
  std::vector<gfx::NativeWindow> windows;
  // TODO(https://crbug.com/405276019): Finalize implementation and add the
  // relevant tests.
  NOTIMPLEMENTED_LOG_ONCE();
  return windows;
}

}  // namespace views
