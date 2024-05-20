// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_MAC_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_MAC_H_

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "ui/events/events_base_export.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {

enum class DomCode : uint32_t;

// We use windows virtual keycodes throughout our keyboard event related code,
// including unit tests. But Mac uses a different set of virtual keycodes.
// This function converts a windows virtual keycode into Mac's virtual key code
// and corresponding unicode character. |flags| is the Cocoa modifiers mask
// such as NSEventModifierFlagControl, NSEventModifierFlagShift, etc.
// When success, the corresponding Mac's virtual key code will be returned.
// |keyboard_character| is the corresponding keyboard character, suitable for
// use in -[NSEvent characters]. If NSEventModifierFlagShift appears in |flags|,
// |us_keyboard_shifted_character| is |keyboard_character| with a shift modifier
// applied using a US keyboard layout (otherwise unmodified).
// |us_keyboard_shifted_character| is suitable for -[NSEvent
// charactersIgnoringModifiers] (which ignores all modifiers except for shift).
// -1 will be returned if the keycode can't be converted.
// This function is mainly for simulating keyboard events in unit tests.
// See |KeyboardCodeFromNSEvent| for reverse conversion.
EVENTS_BASE_EXPORT int MacKeyCodeForWindowsKeyCode(
    KeyboardCode keycode,
    NSUInteger flags,
    unichar* us_keyboard_shifted_character,
    unichar* keyboard_character);

// Returns the WindowsKeyCode from the Mac key code.
EVENTS_BASE_EXPORT KeyboardCode
KeyboardCodeFromKeyCode(unsigned short key_code);

// Returns the KeyboardCode from a |char_code| from AppKit classes.
EVENTS_BASE_EXPORT KeyboardCode KeyboardCodeFromCharCode(unichar char_code);

// This implementation cribbed from:
//   content/browser/render_host/input/web_input_event_builder_mac.mm
// Converts |event| into a |KeyboardCode|.  The mapping is not direct as the Mac
// has a different notion of key codes.
EVENTS_BASE_EXPORT KeyboardCode KeyboardCodeFromNSEvent(NSEvent* event);

EVENTS_BASE_EXPORT DomCode DomCodeFromNSEvent(NSEvent* event);

// Map a |event| to a |DomKey|. |DomKey::NONE| is returned on a failed
// mapping and the callee should may wish to convert this to
// |DomKey::UNIDENTIFIED| before handing the value off.
EVENTS_BASE_EXPORT DomKey DomKeyFromNSEvent(NSEvent* event);

// Map |key_code| to a unicode char based on the params provided.
EVENTS_BASE_EXPORT UniChar
TranslatedUnicodeCharFromKeyCode(TISInputSourceRef input_source,
                                 UInt16 key_code,
                                 UInt16 key_action,
                                 UInt32 modifier_key_state,
                                 UInt32 keyboard_type,
                                 UInt32* dead_key_state);

} // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_MAC_H_
