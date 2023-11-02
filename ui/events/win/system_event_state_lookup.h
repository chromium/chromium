// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_WIN_SYSTEM_EVENT_STATE_LOOKUP_H_
#define UI_EVENTS_WIN_SYSTEM_EVENT_STATE_LOOKUP_H_

#include "ui/events/events_export.h"

namespace ui {
namespace win {

// Returns true if the Shift key is currently pressed.
EVENTS_EXPORT bool IsShiftPressed();

// Returns true if either Control key is pressed (including due to AltGraph).
EVENTS_EXPORT bool IsCtrlPressed();

// Returns true if either Alt key is currently pressed.
EVENTS_EXPORT bool IsAltPressed();

// Returns true if the AltRight (i.e. either Alt or AltGraph) key is pressed.
// This is used in events_win.cc to detect the physical AltGraph key.
EVENTS_EXPORT bool IsAltRightPressed();

// Returns true if the Windows key is currently pressed.
EVENTS_EXPORT bool IsWindowsKeyPressed();

// Returns true if the caps lock state is on.
EVENTS_EXPORT bool IsCapsLockOn();

// Returns true if the num lock state is on.
EVENTS_EXPORT bool IsNumLockOn();

// Returns true if the scroll lock state is on.
EVENTS_EXPORT bool IsScrollLockOn();

}  // namespace win
}  // namespace ui

#endif  // UI_EVENTS_WIN_SYSTEM_EVENT_STATE_LOOKUP_H_
