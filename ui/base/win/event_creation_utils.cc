// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/event_creation_utils.h"

#include <windows.h>

#include <winuser.h>

#include <algorithm>

#include "ui/gfx/geometry/point.h"

namespace ui {

bool SendMouseEvent(const gfx::Point& point, int flags) {
  INPUT input = {INPUT_MOUSE};
  // Get the max screen coordinate for use in computing the normalized absolute
  // coordinates required by SendInput.
  const int screen_left = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
  const int screen_top = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
  const int screen_width = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
  const int screen_height = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

  int screen_x =
      std::clamp(point.x(), screen_left, screen_left + screen_width - 1);
  int screen_y =
      std::clamp(point.y(), screen_top, screen_top + screen_height - 1);

  // In normalized absolute coordinates, (0, 0) maps onto the upper-left corner
  // of the display surface, while (65535, 65535) maps onto the lower-right
  // corner.
  // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-mouse_event#remarks
  static constexpr double kNormalizedScreenSize = 65536.0;

  // Form the input data containing the normalized absolute coordinates. As of
  // Windows 10 Fall Creators Update, moving to an absolute position of zero
  // does not work. It seems that moving to 1,1 does, though.
  input.mi.dx = static_cast<LONG>(
      std::max(1.0, std::ceil((screen_x - screen_left) *
                              (kNormalizedScreenSize / screen_width))));
  input.mi.dy = static_cast<LONG>(
      std::max(1.0, std::ceil((screen_y - screen_top) *
                              (kNormalizedScreenSize / screen_height))));
  input.mi.dwFlags = static_cast<DWORD>(flags | MOUSEEVENTF_ABSOLUTE);
  return ::SendInput(1, &input, sizeof(input)) == 1;
}

}  // namespace ui
