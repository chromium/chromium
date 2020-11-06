// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/fullscreen_win.h"

#include <windows.h>

#include <shellapi.h>

namespace ui {

namespace {

bool IsPlatformFullScreenMode() {
  QUERY_USER_NOTIFICATION_STATE state = {};
  if (FAILED(::SHQueryUserNotificationState(&state)))
    return false;
  return state == QUNS_RUNNING_D3D_FULL_SCREEN ||
         state == QUNS_PRESENTATION_MODE;
}

bool IsFullScreenWindowMode() {
  // Get the foreground window which the user is currently working on.
  HWND wnd = ::GetForegroundWindow();
  if (!wnd)
    return false;

  // Get the monitor where the window is located.
  RECT wnd_rect;
  if (!::GetWindowRect(wnd, &wnd_rect))
    return false;
  HMONITOR monitor = ::MonitorFromRect(&wnd_rect, MONITOR_DEFAULTTONULL);
  if (!monitor)
    return false;
  MONITORINFO monitor_info = {sizeof(monitor_info)};
  if (!::GetMonitorInfo(monitor, &monitor_info))
    return false;

  // It should be the main monitor.
  if (!(monitor_info.dwFlags & MONITORINFOF_PRIMARY))
    return false;

  // The window should be at least as large as the monitor.
  if (!::IntersectRect(&wnd_rect, &wnd_rect, &monitor_info.rcMonitor))
    return false;
  if (!::EqualRect(&wnd_rect, &monitor_info.rcMonitor))
    return false;

  // At last, the window style should not have WS_DLGFRAME and WS_THICKFRAME and
  // its extended style should not have WS_EX_WINDOWEDGE and WS_EX_TOOLWINDOW.
  LONG style = ::GetWindowLong(wnd, GWL_STYLE);
  LONG ext_style = ::GetWindowLong(wnd, GWL_EXSTYLE);
  return !((style & (WS_DLGFRAME | WS_THICKFRAME)) ||
           (ext_style & (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW)));
}

bool IsFullScreenConsoleMode() {
  // We detect this by attaching the current process to the console of the
  // foreground window and then checking if it is in full screen mode.
  DWORD pid = 0;
  ::GetWindowThreadProcessId(::GetForegroundWindow(), &pid);
  if (!pid)
    return false;

  if (!::AttachConsole(pid))
    return false;

  DWORD modes = 0;
  ::GetConsoleDisplayMode(&modes);
  ::FreeConsole();

  return (modes & (CONSOLE_FULLSCREEN | CONSOLE_FULLSCREEN_HARDWARE)) != 0;
}

}  // namespace

bool IsFullScreenMode() {
  return IsPlatformFullScreenMode() || IsFullScreenWindowMode() ||
         IsFullScreenConsoleMode();
}

}  // namespace ui
