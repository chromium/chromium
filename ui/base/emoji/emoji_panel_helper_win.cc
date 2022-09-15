// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/emoji/emoji_panel_helper.h"

#include <windows.h>

#include "base/win/windows_version.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"

namespace ui {

bool IsEmojiPanelSupported() {
  // Emoji picker is supported on Windows 10's Spring 2018 Update and
  // above.
  return base::win::GetVersion() >= base::win::Version::WIN10_RS4;
}

void ShowEmojiPanel() {
  // This sends Windows Key + '.' (both keydown and keyup events).
  // "SendInput" is used because Windows needs to receive these events and
  // open the Emoji picker.
  // TODO(crbug.com/827404): Move to a specialized Windows API once it is
  // available.
  INPUT input[4] = {};
  input[0].type = INPUT_KEYBOARD;
  input[0].ki.wVk = ui::WindowsKeyCodeForKeyboardCode(ui::VKEY_COMMAND);
  input[1].type = INPUT_KEYBOARD;
  input[1].ki.wVk = ui::WindowsKeyCodeForKeyboardCode(ui::VKEY_OEM_PERIOD);

  input[2].type = INPUT_KEYBOARD;
  input[2].ki.wVk = ui::WindowsKeyCodeForKeyboardCode(ui::VKEY_COMMAND);
  input[2].ki.dwFlags |= KEYEVENTF_KEYUP;
  input[3].type = INPUT_KEYBOARD;
  input[3].ki.wVk = ui::WindowsKeyCodeForKeyboardCode(ui::VKEY_OEM_PERIOD);
  input[3].ki.dwFlags |= KEYEVENTF_KEYUP;
  ::SendInput(4, input, sizeof(INPUT));
}

}  // namespace ui
