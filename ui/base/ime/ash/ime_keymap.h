// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_IME_KEYMAP_H_
#define UI_BASE_IME_ASH_IME_KEYMAP_H_

#include <string>

#include "base/component_export.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

// Translates the DOM4 key code string to ui::KeyboardCode.
COMPONENT_EXPORT(UI_BASE_IME_ASH)
ui::KeyboardCode DomKeycodeToKeyboardCode(const std::string& code);

// Translates the ui::KeyboardCode to DOM4 key code string.
COMPONENT_EXPORT(UI_BASE_IME_ASH)
std::string KeyboardCodeToDomKeycode(ui::KeyboardCode code);

}  // namespace ash

#endif  // UI_BASE_IME_ASH_IME_KEYMAP_H_
