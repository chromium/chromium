// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/topmost_window_finder_win.h"

#include "ui/display/win/screen_win.h"

namespace display {
namespace win {

// static
bool TopMostFinderWin::IsTopMostWindowAtPoint(HWND window,
                                              const gfx::Point& screen_loc,
                                              const std::set<HWND>& ignore) {
  TopMostFinderWin finder(window, screen_loc, ignore);
  return finder.is_top_most_;
}

bool TopMostFinderWin::ShouldStopIterating(HWND hwnd) {
  if (hwnd == target_) {
    // Window is topmost, stop iterating.
    is_top_most_ = true;
    return true;
  }

  if (!IsWindowVisible(hwnd)) {
    // The window isn't visible, keep iterating.
    return false;
  }

  RECT r;
  if (!GetWindowRect(hwnd, &r) || !PtInRect(&r, screen_loc_.ToPOINT())) {
    // The window doesn't contain the point, keep iterating.
    return false;
  }

  LONG ex_styles = GetWindowLong(hwnd, GWL_EXSTYLE);
  if (ex_styles & WS_EX_TRANSPARENT || ex_styles & WS_EX_LAYERED) {
    // Mouse events fall through WS_EX_TRANSPARENT windows, so we ignore them.
    //
    // WS_EX_LAYERED is trickier. Apps like Switcher create a totally
    // transparent WS_EX_LAYERED window that is always on top. If we don't
    // ignore WS_EX_LAYERED windows and there are totally transparent
    // WS_EX_LAYERED windows then there are effectively holes on the screen
    // that the user can't reattach tabs to. So we ignore them. This is a bit
    // problematic in so far as WS_EX_LAYERED windows need not be totally
    // transparent in which case we treat chrome windows as not being obscured
    // when they really are, but this is better than not being able to
    // reattach tabs.
    return false;
  }

  // hwnd is at the point. Make sure the point is within the windows region.
  if (GetWindowRgn(hwnd, tmp_region_.get()) == ERROR) {
    // There's no region on the window and the window contains the point. Stop
    // iterating.
    return true;
  }

  // The region is relative to the window's rect.
  BOOL is_point_in_region = PtInRegion(
      tmp_region_.get(), screen_loc_.x() - r.left, screen_loc_.y() - r.top);
  tmp_region_.reset(CreateRectRgn(0, 0, 0, 0));
  // Stop iterating if the region contains the point.
  return !!is_point_in_region;
}

TopMostFinderWin::TopMostFinderWin(HWND window,
                                   const gfx::Point& screen_loc,
                                   const std::set<HWND>& ignore)
    : BaseWindowFinderWin(ignore),
      target_(window),
      is_top_most_(false),
      tmp_region_(CreateRectRgn(0, 0, 0, 0)) {
  screen_loc_ = display::win::ScreenWin::DIPToScreenPoint(screen_loc);
  EnumWindows(WindowCallbackProc, as_lparam());
}

TopMostFinderWin::~TopMostFinderWin() = default;

}  // namespace win
}  // namespace display