// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_UTIL_H_
#define UI_KEYBOARD_KEYBOARD_UTIL_H_

#include <string>

#include "base/strings/string16.h"
#include "ui/keyboard/keyboard_export.h"

// Global utility functions for the virtual keyboard.
// TODO(stevenjb/shuchen/shend): Many of these are accessed from both Chrome
// and Ash. We need to remove any Chrome dependencies. htpps://crbug.com/843332

namespace aura {
class WindowTreeHost;
}

namespace keyboard {

// An enumeration of keyboard states.
enum KeyboardState {
  // Default state. System decides whether to show the keyboard or not.
  KEYBOARD_STATE_AUTO = 0,
  // Request virtual keyboard be deployed.
  KEYBOARD_STATE_ENABLED,
  // Request virtual keyboard be suppressed.
  KEYBOARD_STATE_DISABLED,
};

// Sets the state of the a11y onscreen keyboard.
KEYBOARD_EXPORT void SetAccessibilityKeyboardEnabled(bool enabled);

// Gets the state of the a11y onscreen keyboard.
KEYBOARD_EXPORT bool GetAccessibilityKeyboardEnabled();

// Sets whether the keyboard is enabled from the shelf.
KEYBOARD_EXPORT void SetKeyboardEnabledFromShelf(bool enabled);

// Gets whether the keyboard is enabled from the shelf.
KEYBOARD_EXPORT bool GetKeyboardEnabledFromShelf();

// Sets the state of the touch onscreen keyboard.
KEYBOARD_EXPORT void SetTouchKeyboardEnabled(bool enabled);

// Gets the state of the touch onscreen keyboard.
KEYBOARD_EXPORT bool GetTouchKeyboardEnabled();

// Gets the default keyboard layout.
KEYBOARD_EXPORT std::string GetKeyboardLayout();

// Returns true if the virtual keyboard is enabled.
KEYBOARD_EXPORT bool IsKeyboardEnabled();

// Sends a fabricated key event, where |type| is the event type, |key_value|
// is the unicode value of the character, |key_code| is the legacy key code
// value, |key_name| is the name of the key as defined in the DOM3 key event
// specification, and |modifier| indicates if any modifier keys are being
// virtually pressed. The event is dispatched to the active TextInputClient
// associated with |root_window|. The type may be "keydown" or "keyup".
KEYBOARD_EXPORT bool SendKeyEvent(std::string type,
                                  int key_value,
                                  int key_code,
                                  std::string key_name,
                                  int modifiers,
                                  aura::WindowTreeHost* host);

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_UTIL_H_
