// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_HWND_METRICS_H_
#define UI_BASE_WIN_HWND_METRICS_H_

#include <windows.h>

#include "base/component_export.h"

namespace ui {

// The size, in pixels, of the non-client frame around a window on |monitor|.
COMPONENT_EXPORT(UI_BASE) int GetFrameThickness(HMONITOR monitor);

}  // namespace ui

#endif  // UI_BASE_WIN_HWND_METRICS_H_
