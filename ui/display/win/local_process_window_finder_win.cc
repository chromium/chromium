// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/local_process_window_finder_win.h"

#include "base/win/windows_version.h"
#include "ui/display/win/screen_win.h"
#include "ui/display/win/topmost_window_finder_win.h"

namespace display::win {

// static
gfx::NativeWindow LocalProcessWindowFinder::GetProcessWindowAtPoint(
    const gfx::Point& screen_loc,
    const std::set<HWND>& ignore,
    ScreenWin* screen_win) {
  LocalProcessWindowFinder finder(screen_loc, screen_win, ignore);
  // Windows 8 has a window that appears first in the list of iterated
  // windows, yet is not visually on top of everything.
  // TODO(sky): figure out a better way to ignore this window.
  if (finder.result_ && ((base::win::OSInfo::GetInstance()->version() >=
                          base::win::Version::WIN8) ||
                         TopMostFinderWin::IsTopMostWindowAtPoint(
                             finder.result_, screen_loc, ignore))) {
    return screen_win->GetNativeWindowFromHWND(finder.result_);
  }
  return nullptr;
}

bool LocalProcessWindowFinder::ShouldStopIterating(HWND hwnd) {
  RECT r;
  // Make sure the window is on the same virtual desktop. First check if the
  // host knows if the window is on the current_workspace or not.
  gfx::NativeWindow native_win = screen_win_->GetNativeWindowFromHWND(hwnd);
  absl::optional<bool> on_current_workspace;
  if (native_win) {
    on_current_workspace =
        screen_win_->IsWindowOnCurrentVirtualDesktop(native_win);
  }
  if (on_current_workspace == false)
    return false;

  if (!IsWindowVisible(hwnd) || !GetWindowRect(hwnd, &r) ||
      !PtInRect(&r, screen_loc_.ToPOINT())) {
    return false;  // Window is not at `screen_loc_`.
  }

  // The window is at the correct position on the screen.
  // If we're Win10 or greater, and host doesn't know if the window is on the
  // current virtual desktop, create the a VirtualDesktopManager if we haven't
  // already, and if that succeeds, check if the window is on the current
  // virtual desktop.
  if (base::win::GetVersion() >= base::win::Version::WIN10 &&
      !on_current_workspace.has_value()) {
    // Lazily create virtual_desktop_manager_ since we should rarely need it.
    if (!virtual_desktop_manager_) {
      ::CoCreateInstance(__uuidof(VirtualDesktopManager), nullptr, CLSCTX_ALL,
                         IID_PPV_ARGS(&virtual_desktop_manager_));
    }

    BOOL on_current_desktop;
    if (virtual_desktop_manager_ &&
        SUCCEEDED(virtual_desktop_manager_->IsWindowOnCurrentVirtualDesktop(
            hwnd, &on_current_desktop)) &&
        !on_current_desktop) {
      return false;
    }
  }

  // Don't set result_ if the window is occluded, because there is at least
  // one window covering the browser window. E.g., tab drag drop shouldn't
  // drop on an occluded browser window.
  if (!native_win || !screen_win_->IsNativeWindowOccluded(native_win))
    result_ = hwnd;
  return true;
}

LocalProcessWindowFinder::LocalProcessWindowFinder(const gfx::Point& screen_loc,
                                                   ScreenWin* screen_win,
                                                   const std::set<HWND>& ignore)
    : BaseWindowFinderWin(ignore), result_(nullptr), screen_win_(screen_win) {
  screen_loc_ = display::win::ScreenWin::DIPToScreenPoint(screen_loc);
  EnumThreadWindows(GetCurrentThreadId(), WindowCallbackProc, as_lparam());
}

LocalProcessWindowFinder::~LocalProcessWindowFinder() = default;

}  // namespace display::win
