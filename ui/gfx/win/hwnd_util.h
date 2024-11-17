// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_HWND_UTIL_H_
#define UI_GFX_WIN_HWND_UTIL_H_

#include <shobjidl.h>
#include <windows.h>

#include <wrl/client.h>

#include <optional>
#include <string>

#include "base/component_export.h"

namespace gfx {
class Size;

// A version of the GetClassNameW API that returns the class name in an
// std::wstring. An empty result indicates a failure to get the class name.
COMPONENT_EXPORT(GFX) std::wstring GetClassName(HWND hwnd);

// Useful for subclassing a HWND.  Returns the previous window procedure.
COMPONENT_EXPORT(GFX) WNDPROC SetWindowProc(HWND hwnd, WNDPROC wndproc);

// Pointer-friendly wrappers around Get/SetWindowLong(..., GWLP_USERDATA, ...)
// Returns the previously set value.
COMPONENT_EXPORT(GFX) void* SetWindowUserData(HWND hwnd, void* user_data);
COMPONENT_EXPORT(GFX) void* GetWindowUserData(HWND hwnd);

// Returns true if the specified window is the current active top window or one
// of its children.
COMPONENT_EXPORT(GFX) bool DoesWindowBelongToActiveWindow(HWND window);

// Returns true if the specified window is cloaked.  Windows 10 and later
// have cloaked windows which are windows with WS_VISIBLE attribute but not
// displayed.
COMPONENT_EXPORT(GFX) bool IsWindowCloaked(HWND hwnd);

// Returns true if `window` is on the current virtual desktop, false if isn't,
// and std::nullopt if a COM method fails. Since this calls COM methods,
// it can only be called from a COM thread.
COMPONENT_EXPORT(GFX)
std::optional<bool> IsWindowOnCurrentVirtualDesktop(
    HWND window,
    Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager);

// Sizes the window to have a window size of |pref|, then centers the window
// over |parent|, ensuring the window fits on screen.
COMPONENT_EXPORT(GFX)
void CenterAndSizeWindow(HWND parent, HWND window, const gfx::Size& pref);

// If |hwnd| is nullptr logs various thing and CHECKs. |last_error| must contain
// the result of ::GetLastError(), called immediately after CreateWindow().
COMPONENT_EXPORT(GFX) void CheckWindowCreated(HWND hwnd, DWORD last_error);

// Returns the window you can use to parent a top level window.
// Note that in some cases we create child windows not parented to its final
// container so in those cases you should pass true in |get_real_hwnd|.
COMPONENT_EXPORT(GFX) HWND GetWindowToParentTo(bool get_real_hwnd);

}  // namespace gfx

#endif  // UI_GFX_WIN_HWND_UTIL_H_
