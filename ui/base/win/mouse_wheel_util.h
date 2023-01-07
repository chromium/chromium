// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_MOUSE_WHEEL_UTIL_H_
#define UI_BASE_WIN_MOUSE_WHEEL_UTIL_H_

#include <windows.h>

#include "base/component_export.h"

namespace ui {

class ViewProp;

// Marks the passed |hwnd| as supporting mouse-wheel message rerouting.
// We reroute the mouse wheel messages to such HWND when they are under the
// mouse pointer (but are not the active window). Callers own the returned
// object.
COMPONENT_EXPORT(UI_BASE)
ViewProp* SetWindowSupportsRerouteMouseWheel(HWND hwnd);

// Forwards mouse wheel messages to the window under it.
// Windows sends mouse wheel messages to the currently active window.
// This causes a window to scroll even if it is not currently under the mouse
// wheel. The following code gives mouse wheel messages to the window under the
// mouse wheel in order to scroll that window. This is arguably a better user
// experience.  The returns value says whether the mouse wheel message was
// successfully redirected.
COMPONENT_EXPORT(UI_BASE)
bool RerouteMouseWheel(HWND window, WPARAM w_param, LPARAM l_param);

}  // namespace ui

#endif  // UI_BASE_WIN_MOUSE_WHEEL_UTIL_H_
