// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_HWND_UTIL_H_
#define UI_GFX_WIN_HWND_UTIL_H_

#include <windows.h>

#include "base/strings/string16.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {
class Size;

// A version of the GetClassNameW API that returns the class name in an
// base::string16. An empty result indicates a failure to get the class name.
GFX_EXPORT base::string16 GetClassName(HWND hwnd);

// Useful for subclassing a HWND.  Returns the previous window procedure.
GFX_EXPORT WNDPROC SetWindowProc(HWND hwnd, WNDPROC wndproc);

// Pointer-friendly wrappers around Get/SetWindowLong(..., GWLP_USERDATA, ...)
// Returns the previously set value.
GFX_EXPORT void* SetWindowUserData(HWND hwnd, void* user_data);
GFX_EXPORT void* GetWindowUserData(HWND hwnd);

// Returns true if the specified window is the current active top window or one
// of its children.
GFX_EXPORT bool DoesWindowBelongToActiveWindow(HWND window);

// Sizes the window to have a window size of |pref|, then centers the window
// over |parent|, ensuring the window fits on screen.
GFX_EXPORT void CenterAndSizeWindow(HWND parent,
                                    HWND window,
                                    const gfx::Size& pref);

// If |hwnd| is NULL logs various thing and CHECKs. Invoke right after calling
// CreateWindow.
GFX_EXPORT void CheckWindowCreated(HWND hwnd);

// Returns the window you can use to parent a top level window.
// Note that in some cases we create child windows not parented to its final
// container so in those cases you should pass true in |get_real_hwnd|.
GFX_EXPORT HWND GetWindowToParentTo(bool get_real_hwnd);

}  // namespace gfx

#endif  // UI_GFX_WIN_HWND_UTIL_H_
