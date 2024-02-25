// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_H_

#include "ui/events/events_base_export.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

enum class DomCode : uint32_t;

// Helper functions to get the meaning of a DOM |code| in a
// platform independent way. It supports control characters as well.
// It assumes a US keyboard layout is used, so it may only be used when there
// is no native event or no better way to get the character.
//
// For example, if a virtual keyboard implementation can only generate key
// events with key_code and flags information, then there is no way for us to
// determine the actual character that should be generate by the key. Because
// a key_code only represents a physical key on the keyboard, it has nothing
// to do with the actual character printed on that key. In such case, the only
// thing we can do is to assume that we are using a US keyboard and get the
// character according to US keyboard layout definition. Preferably, such
// events should be created using a full KeyEvent constructor, explicitly
// specifying the character and DOM 3 values as well as the legacy VKEY.

// Helper function to map a physical key state (dom_code and flags)
// to a meaning (dom_key and character, together corresponding to the
// DOM keyboard event |key| value), along with a corresponding non-located
// Windows-based key_code.
//
// This follows a US keyboard layout, so it should only be used when there
// is no other better way to obtain the meaning (e.g. actual keyboard layout).
// Returns true and sets the output parameters if the (dom_code, flags) pair
// has an interpretation in the US English layout; otherwise the output
// parameters are untouched.
EVENTS_BASE_EXPORT char16_t DomCodeToUsLayoutCharacter(DomCode dom_code,
                                                       int flags);
[[nodiscard]] EVENTS_BASE_EXPORT bool DomCodeToUsLayoutDomKey(
    DomCode dom_code,
    int flags,
    DomKey* dom_key,
    KeyboardCode* key_code);

// Helper function to map a physical key (dom_code) to a meaning (dom_key
// and character, together corresponding to the DOM keyboard event |key|
// value), along with a corresponding non-located Windows-based key_code.
// Unlike |DomCodeToUsLayoutDomKey| this function only maps non-printable,
// or action, keys.
[[nodiscard]] EVENTS_BASE_EXPORT bool DomCodeToNonPrintableDomKey(
    DomCode dom_code,
    DomKey* dom_key,
    KeyboardCode* key_code);

// Obtains the control character corresponding to a physical key;
// that is, the meaning of the physical key state (dom_code, and flags
// containing EF_CONTROL_DOWN) under the base US English layout.
// Returns true and sets the output parameters if the (dom_code, flags) pair
// is interpreted as a control character; otherwise the output parameters
// are untouched.
[[nodiscard]] EVENTS_BASE_EXPORT bool DomCodeToControlCharacter(
    DomCode dom_code,
    int flags,
    DomKey* dom_key,
    KeyboardCode* key_code);

// Returns a Windows-based VKEY for a non-printable DOM Level 3 |key|.
// The returned VKEY is non-located (e.g. VKEY_SHIFT).
EVENTS_BASE_EXPORT KeyboardCode
NonPrintableDomKeyToKeyboardCode(DomKey dom_key);

// Determine the non-located VKEY corresponding to a located VKEY.
// Most modifier keys have two kinds of KeyboardCode: located (e.g.
// VKEY_LSHIFT and VKEY_RSHIFT), that indentify one of two specific
// physical keys, and non-located (e.g. VKEY_SHIFT) that identify
// only the operation. Similarly digit keys have a number-pad variant
// (e.g. VKEY_NUMPAD1 on the number pad vs VKEY_1 on the main keyboard),
// except that in this case the main keyboard code doubles as the
// non-located value.
EVENTS_BASE_EXPORT KeyboardCode
LocatedToNonLocatedKeyboardCode(KeyboardCode key_code);

// Determine the located VKEY corresponding to a non-located VKEY.
EVENTS_BASE_EXPORT KeyboardCode
NonLocatedToLocatedKeyboardCode(KeyboardCode key_code, DomCode dom_code);

// Returns a DOM Level 3 |code| from a Windows-based VKEY value.
// This assumes a US layout and should only be used when |code| cannot be
// determined from a physical scan code, for example when a key event was
// generated synthetically by JavaScript with only a VKEY value supplied.
EVENTS_BASE_EXPORT DomCode UsLayoutKeyboardCodeToDomCode(KeyboardCode key_code);

// Returns the Windows-based VKEY value corresponding to a DOM Level 3 |code|,
// assuming a base US English layout. The returned VKEY is located
// (e.g. VKEY_LSHIFT).
EVENTS_BASE_EXPORT KeyboardCode DomCodeToUsLayoutKeyboardCode(DomCode dom_code);

// Returns the Windows-based VKEY value corresponding to a DOM Level 3 |code|,
// assuming a base US English layout. The returned VKEY is non-located
// (e.g. VKEY_SHIFT).
EVENTS_BASE_EXPORT KeyboardCode
DomCodeToUsLayoutNonLocatedKeyboardCode(DomCode dom_code);

// Returns the ui::EventFlags value associated with a modifier key,
// or 0 (EF_NONE) if the key is not a modifier.
EVENTS_BASE_EXPORT int ModifierDomKeyToEventFlag(DomKey key);

// Returns the physical DOM code along with a corresponding non-located
// Windows-based key_code.
EVENTS_BASE_EXPORT DomCode UsLayoutDomKeyToDomCode(DomKey dom_key);

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_H_
