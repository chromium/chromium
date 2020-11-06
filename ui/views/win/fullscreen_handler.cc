// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/fullscreen_handler.h"

#include <memory>

#include "base/win/win_util.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// FullscreenHandler, public:

FullscreenHandler::FullscreenHandler() = default;

FullscreenHandler::~FullscreenHandler() = default;

void FullscreenHandler::SetFullscreen(bool fullscreen) {
  if (fullscreen_ == fullscreen)
    return;

  SetFullscreenImpl(fullscreen);
}

void FullscreenHandler::MarkFullscreen(bool fullscreen) {
  if (!task_bar_list_) {
    HRESULT hr =
        ::CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&task_bar_list_));
    if (SUCCEEDED(hr) && FAILED(task_bar_list_->HrInit()))
      task_bar_list_ = nullptr;
  }

  // As per MSDN marking the window as fullscreen should ensure that the
  // taskbar is moved to the bottom of the Z-order when the fullscreen window
  // is activated. If the window is not fullscreen, the Shell falls back to
  // heuristics to determine how the window should be treated, which means
  // that it could still consider the window as fullscreen. :(
  if (task_bar_list_)
    task_bar_list_->MarkFullscreenWindow(hwnd_, !!fullscreen);
}

gfx::Rect FullscreenHandler::GetRestoreBounds() const {
  return gfx::Rect(saved_window_info_.window_rect);
}

////////////////////////////////////////////////////////////////////////////////
// FullscreenHandler, private:

void FullscreenHandler::SetFullscreenImpl(bool fullscreen) {
  std::unique_ptr<ScopedFullscreenVisibility> visibility;

  // With Aero enabled disabling the visibility causes the window to disappear
  // for several frames, which looks worse than doing other updates
  // non-atomically.
  if (!ui::win::IsAeroGlassEnabled())
    visibility = std::make_unique<ScopedFullscreenVisibility>(hwnd_);

  // Save current window state if not already fullscreen.
  if (!fullscreen_) {
    saved_window_info_.style = GetWindowLong(hwnd_, GWL_STYLE);
    saved_window_info_.ex_style = GetWindowLong(hwnd_, GWL_EXSTYLE);
    GetWindowRect(hwnd_, &saved_window_info_.window_rect);
  }

  fullscreen_ = fullscreen;

  if (fullscreen_) {
    // Set new window style and size.
    SetWindowLong(hwnd_, GWL_STYLE,
                  saved_window_info_.style & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowLong(
        hwnd_, GWL_EXSTYLE,
        saved_window_info_.ex_style & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                                        WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

    // On expand, if we're given a window_rect, grow to it, otherwise do
    // not resize.
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST),
                   &monitor_info);
    gfx::Rect window_rect(monitor_info.rcMonitor);
    SetWindowPos(hwnd_, nullptr, window_rect.x(), window_rect.y(),
                 window_rect.width(), window_rect.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  } else {
    // Reset original window style and size.  The multiple window size/moves
    // here are ugly, but if SetWindowPos() doesn't redraw, the taskbar won't be
    // repainted.  Better-looking methods welcome.
    SetWindowLong(hwnd_, GWL_STYLE, saved_window_info_.style);
    SetWindowLong(hwnd_, GWL_EXSTYLE, saved_window_info_.ex_style);

    // On restore, resize to the previous saved rect size.
    gfx::Rect new_rect(saved_window_info_.window_rect);
    SetWindowPos(hwnd_, nullptr, new_rect.x(), new_rect.y(), new_rect.width(),
                 new_rect.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  }

  MarkFullscreen(fullscreen);
}

}  // namespace views
