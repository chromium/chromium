// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/test/screen_util_win.h"

#include "base/check_op.h"
#include "ui/gfx/geometry/rect.h"

namespace display {
namespace win {
namespace test {

MONITORINFOEX CreateMonitorInfo(const gfx::Rect& monitor,
                                const gfx::Rect& work,
                                const std::wstring& device_name) {
  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  monitor_info.rcMonitor = monitor.ToRECT();
  monitor_info.rcWork = work.ToRECT();
  CHECK_LT(device_name.size(), std::size(monitor_info.szDevice));
  base::span(monitor_info.szDevice).copy_prefix_from(device_name);
  return monitor_info;
}

}  // namespace test
}  // namespace win
}  // namespace display
