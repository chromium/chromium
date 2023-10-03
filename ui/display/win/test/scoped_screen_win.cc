// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/test/scoped_screen_win.h"
#include "ui/display/win/display_info.h"
#include "ui/display/win/test/screen_util_win.h"

namespace display {
namespace win {
namespace test {

ScopedScreenWin::ScopedScreenWin() : ScreenWin(false) {
  constexpr gfx::Rect kPixelBounds(0, 0, 1920, 1200);
  constexpr gfx::Rect kPixelWork(0, 0, 1920, 1100);
  const MONITORINFOEX monitor_info =
      CreateMonitorInfo(kPixelBounds, kPixelWork, L"primary");
  UpdateFromDisplayInfos(
      {{monitor_info, /*device_scale_factor=*/1.0f, 1.0f, Display::ROTATE_0,
        60.0f, gfx::Vector2dF(96.0, 96.0),
        DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER, std::string()}});
}

}  // namespace test
}  // namespace win
}  // namespace display
