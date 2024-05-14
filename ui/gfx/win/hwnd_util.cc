// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/hwnd_util.h"

#include <windows.h>

#include <dwmapi.h>  // DWMWA_CLOAKED

#include "base/debug/gdi_debug_util_win.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/win/win_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {

namespace {

// Adjust the window to fit.
void AdjustWindowToFit(HWND hwnd, const RECT& bounds, bool fit_to_monitor) {
  if (fit_to_monitor) {
    // Get the monitor.
    HMONITOR hmon = MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST);
    if (hmon) {
      MONITORINFO mi;
      mi.cbSize = sizeof(mi);
      GetMonitorInfo(hmon, &mi);
      Rect window_rect(bounds);
      Rect monitor_rect(mi.rcWork);
      Rect new_window_rect = window_rect;
      new_window_rect.AdjustToFit(monitor_rect);
      if (new_window_rect != window_rect) {
        // Window doesn't fit on monitor, move and possibly resize.
        SetWindowPos(hwnd, 0, new_window_rect.x(), new_window_rect.y(),
                     new_window_rect.width(), new_window_rect.height(),
                     SWP_NOACTIVATE | SWP_NOZORDER);
        return;
      }
      // Else fall through.
    } else {
      NOTREACHED_IN_MIGRATION() << "Unable to find default monitor";
      // Fall through.
    }
  }  // Else fall through.

  // The window is not being fit to monitor, or the window fits on the monitor
  // as is, or we have no monitor info; reset the bounds.
  ::SetWindowPos(hwnd, 0, bounds.left, bounds.top,
                 bounds.right - bounds.left, bounds.bottom - bounds.top,
                 SWP_NOACTIVATE | SWP_NOZORDER);
}

// Don't inline these functions so they show up in crash reports.

NOINLINE void CrashAccessDenied(DWORD last_error) {
  LOG(FATAL) << last_error;
}

// Crash isn't one of the ones we commonly see.
NOINLINE void CrashOther(DWORD last_error) {
  LOG(FATAL) << last_error;
}

}  // namespace

std::wstring GetClassName(HWND window) {
  // GetClassNameW will return a truncated result (properly null terminated) if
  // the given buffer is not large enough.  So, it is not possible to determine
  // that we got the entire class name if the result is exactly equal to the
  // size of the buffer minus one.
  DWORD buffer_size = MAX_PATH;
  while (true) {
    std::wstring output;
    DWORD size_ret = GetClassNameW(
        window, base::WriteInto(&output, buffer_size), buffer_size);
    if (size_ret == 0)
      break;
    if (size_ret < (buffer_size - 1)) {
      output.resize(size_ret);
      return output;
    }
    buffer_size *= 2;
  }
  return std::wstring();  // error
}

#pragma warning(push)
#pragma warning(disable:4312 4244)

WNDPROC SetWindowProc(HWND hwnd, WNDPROC proc) {
  // The reason we don't return the SetwindowLongPtr() value is that it returns
  // the orignal window procedure and not the current one. I don't know if it is
  // a bug or an intended feature.
  WNDPROC oldwindow_proc =
      reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_WNDPROC));
  SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(proc));
  return oldwindow_proc;
}

void* SetWindowUserData(HWND hwnd, void* user_data) {
  return
      reinterpret_cast<void*>(SetWindowLongPtr(hwnd, GWLP_USERDATA,
          reinterpret_cast<LONG_PTR>(user_data)));
}

void* GetWindowUserData(HWND hwnd) {
  DWORD process_id = 0;
  GetWindowThreadProcessId(hwnd, &process_id);
  // A window outside the current process needs to be ignored.
  if (process_id != ::GetCurrentProcessId())
    return NULL;
  return reinterpret_cast<void*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

bool IsWindowCloaked(HWND hwnd) {
  BOOL is_cloaked = FALSE;
  return SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &is_cloaked,
                                         sizeof(is_cloaked))) &&
         is_cloaked;
}

std::optional<bool> IsWindowOnCurrentVirtualDesktop(
    HWND window,
    Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager) {
  BOOL on_current_desktop;
  if (FAILED(virtual_desktop_manager->IsWindowOnCurrentVirtualDesktop(
          window, &on_current_desktop))) {
    return std::nullopt;
  }
  if (on_current_desktop)
    return true;

  // IsWindowOnCurrentVirtualDesktop() is flaky for newly opened windows,
  // which causes test flakiness. Occasionally, it incorrectly says a window
  // is not on the current virtual desktop when it is. In this situation,
  // it also returns GUID_NULL for the desktop id.
  GUID workspace_guid;
  if (FAILED(virtual_desktop_manager->GetWindowDesktopId(window,
                                                         &workspace_guid))) {
    return std::nullopt;
  }
  return workspace_guid == GUID_NULL;
}

#pragma warning(pop)

bool DoesWindowBelongToActiveWindow(HWND window) {
  DCHECK(window);
  HWND top_window = ::GetAncestor(window, GA_ROOT);
  if (!top_window)
    return false;

  HWND active_top_window = ::GetAncestor(::GetForegroundWindow(), GA_ROOT);
  return (top_window == active_top_window);
}

void CenterAndSizeWindow(HWND parent,
                         HWND window,
                         const Size& pref) {
  DCHECK(window && pref.width() > 0 && pref.height() > 0);

  // Calculate the ideal bounds.
  RECT window_bounds;
  RECT center_bounds = {0};
  if (parent) {
    // If there is a parent, center over the parents bounds.
    ::GetWindowRect(parent, &center_bounds);
  }

  if (::IsRectEmpty(&center_bounds)) {
    // No parent or no parent rect. Center over the monitor the window is on.
    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    if (monitor) {
      MONITORINFO mi = {0};
      mi.cbSize = sizeof(mi);
      GetMonitorInfo(monitor, &mi);
      center_bounds = mi.rcWork;
    } else {
      NOTREACHED_IN_MIGRATION() << "Unable to get default monitor";
    }
  }

  window_bounds.left = center_bounds.left;
  if (pref.width() < (center_bounds.right - center_bounds.left)) {
    window_bounds.left +=
        (center_bounds.right - center_bounds.left - pref.width()) / 2;
  }
  window_bounds.right = window_bounds.left + pref.width();

  window_bounds.top = center_bounds.top;
  if (pref.height() < (center_bounds.bottom - center_bounds.top)) {
    window_bounds.top +=
        (center_bounds.bottom - center_bounds.top - pref.height()) / 2;
  }
  window_bounds.bottom = window_bounds.top + pref.height();

  // If we're centering a child window, we are positioning in client
  // coordinates, and as such we need to offset the target rectangle by the
  // position of the parent window.
  if (::GetWindowLong(window, GWL_STYLE) & WS_CHILD) {
    DCHECK(parent && ::GetParent(window) == parent);
    POINT topleft = { window_bounds.left, window_bounds.top };
    ::MapWindowPoints(HWND_DESKTOP, parent, &topleft, 1);
    window_bounds.left = topleft.x;
    window_bounds.top = topleft.y;
    window_bounds.right = window_bounds.left + pref.width();
    window_bounds.bottom = window_bounds.top + pref.height();
  }

  AdjustWindowToFit(window, window_bounds, !parent);
}

void CheckWindowCreated(HWND hwnd, DWORD last_error) {
  if (!hwnd) {
    switch (last_error) {
      case ERROR_NOT_ENOUGH_MEMORY:
      case ERROR_NO_MORE_USER_HANDLES:
        base::debug::CollectGDIUsageAndDie();
        break;
      case ERROR_ACCESS_DENIED:
        CrashAccessDenied(last_error);
        break;
      default:
        CrashOther(last_error);
        break;
    }
    LOG(FATAL) << last_error;
  }
}

extern "C" {
  typedef HWND (*RootWindow)();
}

HWND GetWindowToParentTo(bool get_real_hwnd) {
  return get_real_hwnd ? ::GetDesktopWindow() : HWND_DESKTOP;
}

}  // namespace gfx
