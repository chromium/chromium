// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/test/screen_util_win.h"

#include <cwchar>

#include "ui/gfx/geometry/rect.h"

namespace display {
namespace win {
namespace test {

MONITORINFOEX CreateMonitorInfo(const gfx::Rect& monitor,
                                const gfx::Rect& work,
                                const std::wstring& device_name) {
  MONITORINFOEX monitor_info;
  ::ZeroMemory(&monitor_info, sizeof(monitor_info));
  monitor_info.cbSize = sizeof(monitor_info);
  monitor_info.rcMonitor = monitor.ToRECT();
  monitor_info.rcWork = work.ToRECT();
  size_t device_char_count = ARRAYSIZE(monitor_info.szDevice);
  wcsncpy(monitor_info.szDevice, device_name.c_str(), device_char_count);
  monitor_info.szDevice[device_char_count-1] = L'\0';
  return monitor_info;
}

}  // namespace test
}  // namespace win
}  // namespace display
