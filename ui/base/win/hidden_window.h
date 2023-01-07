// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_HIDDEN_WINDOW_H_
#define UI_BASE_WIN_HIDDEN_WINDOW_H_

#include <windows.h>

#include "base/component_export.h"

namespace ui {

// Returns an HWND that can be used as a temporary parent. The returned HWND is
// never destroyed.
COMPONENT_EXPORT(UI_BASE) HWND GetHiddenWindow();

}  // namespace ui

#endif  // UI_BASE_WIN_HIDDEN_WINDOW_H_
