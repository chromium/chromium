// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_menu_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/keycodes/keysym_to_unicode.h"
#include "ui/events/x/events_x_utils.h"

namespace ui {

X11MenuUtils::X11MenuUtils() = default;

X11MenuUtils::~X11MenuUtils() = default;

int X11MenuUtils::GetCurrentKeyModifiers() const {
  return GetModifierKeyState();
}

std::string X11MenuUtils::ToDBusKeySym(KeyboardCode code) const {
  return base::UTF16ToUTF8(
      std::u16string(1, ui::GetUnicodeCharacterFromXKeySym(
                            XKeysymForWindowsKeyCode(code, false))));
}

}  // namespace ui
