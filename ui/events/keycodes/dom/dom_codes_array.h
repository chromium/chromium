// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_DOM_DOM_CODES_ARRAY_H_
#define UI_EVENTS_KEYCODES_DOM_DOM_CODES_ARRAY_H_

#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {

// Declares an array containing all the currently defined DomCode values.
#define DOM_CODE_TYPE(x) static_cast<DomCode>(x)
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) DOM_CODE_TYPE(usb)
#define DOM_CODE_DECLARATION constexpr DomCode kDomCodesArray[] =
#include "ui/events/keycodes/dom/dom_code_data.inc"
#undef DOM_CODE_TYPE
#undef DOM_CODE
#undef DOM_CODE_DECLARATION

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_DOM_DOM_CODES_ARRAY_H_
