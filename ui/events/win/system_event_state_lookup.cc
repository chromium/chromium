// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/win/system_event_state_lookup.h"

#include <windows.h>

namespace ui {
namespace win {

bool IsShiftPressed() {
  return (::GetKeyState(VK_SHIFT) & 0x8000) == 0x8000;
}

bool IsCtrlPressed() {
  return (::GetKeyState(VK_CONTROL) & 0x8000) == 0x8000;
}

bool IsAltPressed() {
  return (::GetKeyState(VK_MENU) & 0x8000) == 0x8000;
}

bool IsAltRightPressed() {
  return (::GetKeyState(VK_RMENU) & 0x8000) == 0x8000;
}

bool IsWindowsKeyPressed() {
  return (::GetKeyState(VK_LWIN) & 0x8000) == 0x8000 ||
         (::GetKeyState(VK_RWIN) & 0x8000) == 0x8000;
}

bool IsCapsLockOn() {
  return (::GetKeyState(VK_CAPITAL) & 0x0001) == 0x0001;
}

bool IsNumLockOn() {
  return (::GetKeyState(VK_NUMLOCK) & 0x0001) == 0x0001;
}

bool IsScrollLockOn() {
  return (::GetKeyState(VK_SCROLL) & 0x0001) == 0x0001;
}

}  // namespace win
}  // namespace ui
