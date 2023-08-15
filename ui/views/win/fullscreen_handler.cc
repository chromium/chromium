// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/fullscreen_handler.h"

#include <memory>

#include "base/win/win_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/win/screen_win.h"
#include "ui/display/win/screen_win_display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// FullscreenHandler, public:

FullscreenHandler::FullscreenHandler() = default;

FullscreenHandler::~FullscreenHandler() = default;

void FullscreenHandler::SetFullscreen(bool fullscreen,
                                      int64_t target_display_id) {
  if (fullscreen_ == fullscreen &&
      target_display_id == display::kInvalidDisplayId) {
    return;
  }

  ProcessFullscreen(fullscreen, target_display_id);
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
  return gfx::Rect(saved_window_info_.rect);
}

////////////////////////////////////////////////////////////////////////////////
// FullscreenHandler, private:

void FullscreenHandler::ProcessFullscreen(bool fullscreen,
                                          int64_t target_display_id) {
  std::unique_ptr<ScopedFullscreenVisibility> visibility;

  // Save current window state if not already fullscreen.
  if (!fullscreen_) {
    saved_window_info_.style = GetWindowLong(hwnd_, GWL_STYLE);
    saved_window_info_.ex_style = GetWindowLong(hwnd_, GWL_EXSTYLE);
    // Store the original window rect, DPI, and monitor info to detect changes
    // and more accurately restore window placements when exiting fullscreen.
    ::GetWindowRect(hwnd_, &saved_window_info_.rect);
    saved_window_info_.dpi = display::win::ScreenWin::GetDPIForHWND(hwnd_);
    saved_window_info_.monitor =
        MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    saved_window_info_.monitor_info.cbSize =
        sizeof(saved_window_info_.monitor_info);
    GetMonitorInfo(saved_window_info_.monitor,
                   &saved_window_info_.monitor_info);
  }

  fullscreen_ = fullscreen;

  auto ref = weak_ptr_factory_.GetWeakPtr();
  if (fullscreen_) {
    // Set new window style and size.
    SetWindowLong(hwnd_, GWL_STYLE,
                  saved_window_info_.style & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowLong(
        hwnd_, GWL_EXSTYLE,
        saved_window_info_.ex_style & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                                        WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

    // Set the window rect to the rcMonitor of the targeted or current display.
    const display::win::ScreenWinDisplay screen_win_display =
        display::win::ScreenWin::GetScreenWinDisplayWithDisplayId(
            target_display_id);
    gfx::Rect window_rect = screen_win_display.screen_rect();
    if (target_display_id == display::kInvalidDisplayId ||
        screen_win_display.display().id() != target_display_id) {
      HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
      MONITORINFO monitor_info;
      monitor_info.cbSize = sizeof(monitor_info);
      GetMonitorInfo(monitor, &monitor_info);
      window_rect = gfx::Rect(monitor_info.rcMonitor);
    }
    SetWindowPos(hwnd_, nullptr, window_rect.x(), window_rect.y(),
                 window_rect.width(), window_rect.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  } else {
    // Restore the window style and bounds saved prior to entering fullscreen.
    // Use WS_VISIBLE for windows shown after SetFullscreen: crbug.com/1062251.
    // Making multiple window adjustments here is ugly, but if SetWindowPos()
    // doesn't redraw, the taskbar won't be repainted.
    SetWindowLong(hwnd_, GWL_STYLE, saved_window_info_.style | WS_VISIBLE);
    SetWindowLong(hwnd_, GWL_EXSTYLE, saved_window_info_.ex_style);

    gfx::Rect window_rect(saved_window_info_.rect);
    HMONITOR monitor =
        MonitorFromRect(&saved_window_info_.rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfo(monitor, &monitor_info);
    // Adjust the window bounds to restore, if displays were disconnected,
    // virtually rearranged, or otherwise changed metrics during fullscreen.
    if (monitor != saved_window_info_.monitor ||
        gfx::Rect(saved_window_info_.monitor_info.rcWork) !=
            gfx::Rect(monitor_info.rcWork)) {
      window_rect.AdjustToFit(gfx::Rect(monitor_info.rcWork));
    }
    const int fullscreen_dpi = display::win::ScreenWin::GetDPIForHWND(hwnd_);

    SetWindowPos(hwnd_, nullptr, window_rect.x(), window_rect.y(),
                 window_rect.width(), window_rect.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    const int final_dpi = display::win::ScreenWin::GetDPIForHWND(hwnd_);
    if (final_dpi != saved_window_info_.dpi || final_dpi != fullscreen_dpi) {
      // Reissue SetWindowPos if the DPI changed from saved or fullscreen DPIs.
      // The first call may misinterpret bounds spanning displays, if the
      // fullscreen display's DPI does not match the target display's DPI.
      //
      // Scale and clamp the bounds if the final DPI changed from the saved DPI.
      // This more accurately matches the original placement, while avoiding
      // unexpected offscreen placement in a recongifured multi-screen space.
      if (final_dpi != saved_window_info_.dpi) {
        gfx::SizeF size(window_rect.size());
        size.Scale(final_dpi / static_cast<float>(saved_window_info_.dpi));
        window_rect.set_size(gfx::ToCeiledSize(size));
        window_rect.AdjustToFit(gfx::Rect(monitor_info.rcWork));
      }
      SetWindowPos(hwnd_, nullptr, window_rect.x(), window_rect.y(),
                   window_rect.width(), window_rect.height(),
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
  }
  if (!ref)
    return;

  MarkFullscreen(fullscreen);
}

}  // namespace views
