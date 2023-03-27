// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_FUCHSIA_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_FUCHSIA_H_

#include "ui/events/events_base_export.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace fuchsia_ui_input3 {
class KeyMeaning;
}  // namespace fuchsia_ui_input3

namespace ui {

// Converts a Fuchsia KeyMeaning to a DomKey.
EVENTS_BASE_EXPORT DomKey
DomKeyFromFuchsiaKeyMeaning(const fuchsia_ui_input3::KeyMeaning& key_meaning);

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_FUCHSIA_H_