// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_X_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_X_H_

#include <stdint.h>


#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/keycodes/keycodes_x_export.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"

namespace ui {

enum class DomCode : uint32_t;

KEYCODES_X_EXPORT KeyboardCode KeyboardCodeFromXKeyEvent(const x11::Event& xev);

KEYCODES_X_EXPORT KeyboardCode KeyboardCodeFromXKeysym(unsigned int keysym);

KEYCODES_X_EXPORT DomCode CodeFromXEvent(const x11::Event& xev);

// Returns a character on a standard US PC keyboard from an XEvent.
KEYCODES_X_EXPORT uint16_t GetCharacterFromXEvent(const x11::Event& xev);

// Returns DomKey and character from an XEvent.
KEYCODES_X_EXPORT DomKey GetDomKeyFromXEvent(const x11::Event& xev);

// Converts a KeyboardCode into an X KeySym.
KEYCODES_X_EXPORT int XKeysymForWindowsKeyCode(KeyboardCode keycode,
                                               bool shift);

// Returns a XKeyEvent keycode (scancode) for a KeyboardCode. Keyboard layouts
// are usually not injective, so inverse mapping should be avoided when
// practical. A round-trip keycode -> KeyboardCode -> keycode will not
// necessarily return the original keycode.
KEYCODES_X_EXPORT unsigned int XKeyCodeForWindowsKeyCode(
    KeyboardCode key_code,
    int flags,
    x11::Connection* connection);

// Converts an X keycode into ui::KeyboardCode.
KEYCODES_X_EXPORT KeyboardCode
DefaultKeyboardCodeFromHardwareKeycode(unsigned int hardware_code);

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_X_H_
