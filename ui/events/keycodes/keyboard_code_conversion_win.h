// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_WIN_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_WIN_H_

#include "base/win/windows_types.h"
#include "ui/events/events_base_export.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

// If bit 2 (0x04) is set in `wFlags`, `ToUnicodeEx()` does not change the
// kernel keyboard state (supported since Windows 10 version 1607). Without this
// flag, querying a dead key mutates the desktop-wide win32k dead-key state and
// races with concurrent `ToUnicodeEx()` calls on the same desktop.
inline constexpr UINT kDoNotChangeKeyboardState = 0x04;

// Methods to convert ui::KeyboardCode/Windows virtual key type methods.
EVENTS_BASE_EXPORT WORD WindowsKeyCodeForKeyboardCode(KeyboardCode keycode);
EVENTS_BASE_EXPORT KeyboardCode KeyboardCodeForWindowsKeyCode(WORD keycode);
EVENTS_BASE_EXPORT DomCode CodeForWindowsScanCode(WORD scan_code);

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_WIN_H_
