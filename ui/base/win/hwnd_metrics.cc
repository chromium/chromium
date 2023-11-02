// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/hwnd_metrics.h"

#include <windows.h>

#include "ui/display/win/screen_win.h"

namespace ui {

int GetFrameThickness(HMONITOR monitor) {
  // On Windows 10 the visible frame border is one pixel thick, but there is
  // some additional non-visible space: SM_CXSIZEFRAME (the resize handle)
  // and SM_CXPADDEDBORDER (additional border space that isn't part of the
  // resize handle).
  const int resize_frame_thickness =
      display::win::ScreenWin::GetSystemMetricsForMonitor(monitor,
                                                          SM_CXSIZEFRAME);
  const int padding_thickness =
      display::win::ScreenWin::GetSystemMetricsForMonitor(monitor,
                                                          SM_CXPADDEDBORDER);
  return resize_frame_thickness + padding_thickness;
}

}  // namespace ui
