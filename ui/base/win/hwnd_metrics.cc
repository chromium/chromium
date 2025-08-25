// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/hwnd_metrics.h"

#include "ui/display/screen.h"
#include "ui/display/win/screen_win.h"
#include "ui/display/win/screen_win_headless.h"

namespace ui {

namespace {

int GetFrameThicknessFromDisplayId(int64_t id) {
  const int resize_frame_thickness =
      display::win::GetScreenWinHeadless()->GetSystemMetricsForDisplayId(
          id, SM_CXSIZEFRAME);
  const int padding_thickness =
      display::win::GetScreenWinHeadless()->GetSystemMetricsForDisplayId(
          id, SM_CXPADDEDBORDER);

  return resize_frame_thickness + padding_thickness;
}

}  // namespace

int GetResizableFrameThicknessFromMonitorInPixels(HMONITOR monitor,
                                                  bool has_caption) {
  const int resize_handle_thickness =
      display::win::GetScreenWin()->GetSystemMetricsForMonitor(monitor,
                                                               SM_CXSIZEFRAME);
  // SM_CXPADDEDBORDER is some extra padding not part of the resize handle.
  const int padding_thickness =
      display::win::GetScreenWin()->GetSystemMetricsForMonitor(
          monitor, SM_CXPADDEDBORDER);
  // If a window has WS_CAPTION set the frame thickness includes a 1px border.
  // This border must be removed if WS_CAPTION is not set.
  return resize_handle_thickness + padding_thickness - (has_caption ? 0 : 1);
}

int GetResizableFrameThicknessFromMonitorInDIP(HMONITOR monitor,
                                               bool has_caption) {
  return GetResizableFrameThicknessFromMonitorInPixels(monitor, has_caption) /
         display::win::GetScreenWin()->GetScaleFactorForMonitor(monitor);
}

int GetFrameThicknessFromWindow(HWND hwnd, DWORD default_options) {
  if (display::Screen::Get()->IsHeadless()) {
    return GetFrameThicknessFromDisplayId(
        display::win::GetScreenWinHeadless()->GetDisplayIdFromWindow(
            hwnd, default_options));
  } else {
    HMONITOR monitor = ::MonitorFromWindow(hwnd, default_options);
    return GetResizableFrameThicknessFromMonitorInPixels(
        monitor, GetWindowLong(hwnd, GWL_STYLE) & WS_CAPTION);
  }
}

int GetFrameThicknessFromScreenRect(const gfx::Rect& screen_rect) {
  if (display::Screen::Get()->IsHeadless()) {
    return GetFrameThicknessFromDisplayId(
        display::win::GetScreenWinHeadless()->GetDisplayIdFromScreenRect(
            screen_rect));
  } else {
    // This should never be used in headful mode due to the lack of support by
    // display::win::ScreenWin.
    NOTREACHED();
  }
}

}  // namespace ui
