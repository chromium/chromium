// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_ANDROID_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_ANDROID_H_

#include "ui/events/events_base_export.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {

EVENTS_BASE_EXPORT KeyboardCode KeyboardCodeFromAndroidKeyCode(int keycode);

// Returns DomKey from an Android character and keycode.
EVENTS_BASE_EXPORT DomKey GetDomKeyFromAndroidEvent(int keycode,
                                                    int unicode_character);

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_ANDROID_H_
