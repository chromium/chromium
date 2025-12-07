// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_HWND_METRICS_H_
#define UI_BASE_WIN_HWND_METRICS_H_

#include <windows.h>

#include "base/component_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

// Returns the thickness, in screen pixels, of the non-client frame's left,
// right, and bottom borders around a resizble (WS_THICKFRAME) window on the
// given monitor. This frame consists of both:
//  - A non-visible resize handle.
//  - A visible border.
// This thickness *excludes* the top border (title bar), which is typically
// thicker than other borders.
// This function assumes the window has WS_THICKFRAME style.
// `has_caption` means the window has the WS_CAPTION style, which adds adds 1px
// to frame thickness.
COMPONENT_EXPORT(UI_BASE)
int GetResizableFrameThicknessFromMonitorInPixels(HMONITOR monitor,
                                                  bool has_caption);

// Returns the resizable frame thickness in DIP. Note, because of rounding
// errors this may be 1 DIP off from the actual resize frame thickness.
COMPONENT_EXPORT(UI_BASE)
int GetResizableFrameThicknessFromMonitorInDIP(HMONITOR monitor,
                                               bool has_caption);

// Returns the above given the window handle. Note that during WM_NCCALCSIZE
// Windows does not return the correct monitor for the HWND, so it must be
// passed in explicitly (see HWNDMessageHandler::OnNCCalcSize for more details).
// See Win32 MonitorFromWindow API for the available |default_options|.
COMPONENT_EXPORT(UI_BASE)
int GetFrameThicknessFromWindow(HWND hwnd, DWORD default_options);

// Returns the above given the screen rectangle. This is intended to be used
// only in Chrome Headless Mode.
COMPONENT_EXPORT(UI_BASE)
int GetFrameThicknessFromScreenRect(const gfx::Rect& screen_rect);

}  // namespace ui

#endif  // UI_BASE_WIN_HWND_METRICS_H_
