// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_XKB_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_XKB_H_

// These functions are used by X11 targets and by Ozone/XKBcommon targets.


#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/xkb_keysym.h"

namespace ui {

// Returns the DomKey associated with a non-character xkb_keysym_t.
// Returns DomKey::NONE for unrecognized keysyms, which includes
// all printable characters.
DomKey NonPrintableXKeySymToDomKey(xkb_keysym_t keysym);

// TODO(kpschoedel) crbug.com/442757
// Returns the dead key combining character associated with an xkb_keysym_t,
// or 0 if the keysym is not recognized.
// char16_t DeadXKeySymToCombiningCharacter(xkb_keysym_t keysym);

// Return the DomKey determined by the XKB layout result (keysym, character).
DomKey XKeySymToDomKey(xkb_keysym_t keysym, char16_t character);

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_XKB_H_
