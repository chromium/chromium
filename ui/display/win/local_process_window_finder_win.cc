// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/local_process_window_finder_win.h"

#include "base/win/windows_version.h"
#include "ui/display/win/screen_win.h"
#include "ui/display/win/topmost_window_finder_win.h"

namespace display {
namespace win {

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

  // Make sure the window is on the same virtual desktop.
  if (virtual_desktop_manager_) {
    BOOL on_current_desktop;
    if (SUCCEEDED(virtual_desktop_manager_->IsWindowOnCurrentVirtualDesktop(
            hwnd, &on_current_desktop)) &&
        !on_current_desktop) {
      return false;
    }
  }

  if (IsWindowVisible(hwnd) && GetWindowRect(hwnd, &r) &&
      PtInRect(&r, screen_loc_.ToPOINT())) {
    // Don't set result_ if the window is occluded, because there is at least
    // one window covering the browser window. E.g., tab drag drop shouldn't
    // drop on an occluded browser window.
    gfx::NativeWindow native_window =
        screen_win_->GetNativeWindowFromHWND(hwnd);
    if (!native_window || !screen_win_->IsNativeWindowOccluded(native_window))
      result_ = hwnd;
    return true;
  }
  return false;
}

LocalProcessWindowFinder::LocalProcessWindowFinder(const gfx::Point& screen_loc,
                                                   ScreenWin* screen_win,
                                                   const std::set<HWND>& ignore)
    : BaseWindowFinderWin(ignore), result_(nullptr), screen_win_(screen_win) {
  if (base::win::GetVersion() >= base::win::Version::WIN10) {
    ::CoCreateInstance(__uuidof(VirtualDesktopManager), nullptr, CLSCTX_ALL,
                       IID_PPV_ARGS(&virtual_desktop_manager_));
  }
  screen_loc_ = display::win::ScreenWin::DIPToScreenPoint(screen_loc);
  EnumThreadWindows(GetCurrentThreadId(), WindowCallbackProc, as_lparam());
}

LocalProcessWindowFinder::~LocalProcessWindowFinder() = default;

}  // namespace win
}  // namespace display