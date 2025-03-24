// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_HWND_METRICS_H_
#define UI_BASE_WIN_HWND_METRICS_H_

#include <windows.h>

#include "base/component_export.h"

namespace ui {

// Returns the thickness, in pixels, of the non-client frame's left, right, and
// bottom borders around a resizble (WS_THICKFRAME) window on the given monitor.
// This frame consists of:
//  - A non-visible resize handle.
//  - A visible border.
// This thickness *excludes* the top border (title bar), which is typically
// thicker than other borders.
// This function assumes the window has WS_THICKFRAME and WS_CAPTION styles.
// WS_CAPTION style adds 1px to frame thickness.
// TODO(kerenzhu): this should be renamed to GetResizableFrameThickness().
COMPONENT_EXPORT(UI_BASE) int GetFrameThickness(HMONITOR monitor);

}  // namespace ui

#endif  // UI_BASE_WIN_HWND_METRICS_H_
