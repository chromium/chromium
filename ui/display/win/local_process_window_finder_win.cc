// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/local_process_window_finder_win.h"

#include "base/win/windows_version.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/win/hwnd_util.h"

namespace display::win {

// static
gfx::NativeWindow LocalProcessWindowFinder::GetProcessWindowAtPoint(
    const gfx::Point& screen_loc,
    const std::set<HWND>& ignore,
    ScreenWin* screen_win) {
  LocalProcessWindowFinder finder(screen_loc, screen_win, ignore);
  return finder.result_ ? screen_win->GetNativeWindowFromHWND(finder.result_)
                        : nullptr;
}

bool LocalProcessWindowFinder::ShouldStopIterating(HWND hwnd) {
  // If the host knows `hwnd` is not on the current_workspace, return.
  gfx::NativeWindow native_win = screen_win_->GetNativeWindowFromHWND(hwnd);
  std::optional<bool> on_current_workspace;
  if (native_win) {
    on_current_workspace =
        screen_win_->IsWindowOnCurrentVirtualDesktop(native_win);
  }
  if (on_current_workspace == false)
    return false;

  // Ignore non visible  and cloaked windows. This will include windows not on
  // the current virtual desktop, which are cloaked.
  RECT r;
  if (!IsWindowVisible(hwnd) || gfx::IsWindowCloaked(hwnd) ||
      !GetWindowRect(hwnd, &r) || !PtInRect(&r, screen_loc_.ToPOINT())) {
    return false;  // Window is not at `screen_loc_`.
  }

  // The window is at the correct position on the screen.
  // Don't set `result_` if the window is occluded, because there is at least
  // one window covering the browser window. E.g., tab drag drop shouldn't
  // drop on an occluded browser window.
  if (!native_win || !screen_win_->IsNativeWindowOccluded(native_win))
    result_ = hwnd;
  return true;
}

LocalProcessWindowFinder::LocalProcessWindowFinder(const gfx::Point& screen_loc,
                                                   ScreenWin* screen_win,
                                                   const std::set<HWND>& ignore)
    : BaseWindowFinderWin(ignore), screen_win_(screen_win) {
  screen_loc_ = display::win::ScreenWin::DIPToScreenPoint(screen_loc);
  EnumThreadWindows(GetCurrentThreadId(), WindowCallbackProc, as_lparam());
}

LocalProcessWindowFinder::~LocalProcessWindowFinder() = default;

}  // namespace display::win
