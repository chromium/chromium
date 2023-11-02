// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_DOM_DOM_KEYBOARD_LAYOUT_MAP_H_
#define UI_EVENTS_KEYCODES_DOM_DOM_KEYBOARD_LAYOUT_MAP_H_

#include <string>

#include "base/containers/flat_map.h"

namespace ui {

// Generates a map representing the physical keys on the current keyboard
// layout.  Each entry maps a DomCode string to a DomKey string.  The current
// layout is determined by the underlying OS platform which may be the active
// layout or the first ASCII capable layout available.
// More info at: https://wicg.github.io/keyboard-map/
base::flat_map<std::string, std::string> GenerateDomKeyboardLayoutMap();

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_DOM_DOM_KEYBOARD_LAYOUT_MAP_H_
