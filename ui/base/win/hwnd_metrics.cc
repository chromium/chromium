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

int GetResizeFrameOnlyThickness(HMONITOR monitor) {
  return display::win::GetScreenWin()->GetSystemMetricsForMonitor(
      monitor, SM_CXSIZEFRAME);
}

int GetFrameThickness(HMONITOR monitor, bool has_caption) {
  // On Windows 10 the visible frame border is one pixel thick, but there is
  // some additional non-visible space: SM_CXSIZEFRAME (the resize handle)
  // and SM_CXPADDEDBORDER (additional border space that isn't part of the
  // resize handle).
  const int resize_frame_thickness = GetResizeFrameOnlyThickness(monitor);
  const int padding_thickness =
      display::win::GetScreenWin()->GetSystemMetricsForMonitor(
          monitor, SM_CXPADDEDBORDER);
  return resize_frame_thickness + padding_thickness - (has_caption ? 0 : 1);
}

int GetFrameThicknessFromWindow(HWND hwnd, DWORD default_options) {
  if (display::Screen::GetScreen()->IsHeadless()) {
    return GetFrameThicknessFromDisplayId(
        display::win::GetScreenWinHeadless()->GetDisplayIdFromWindow(
            hwnd, default_options));
  } else {
    HMONITOR monitor = ::MonitorFromWindow(hwnd, default_options);
    return GetFrameThickness(monitor,
                             GetWindowLong(hwnd, GWL_STYLE) & WS_CAPTION);
  }
}

int GetFrameThicknessFromScreenRect(const gfx::Rect& screen_rect) {
  if (display::Screen::GetScreen()->IsHeadless()) {
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
