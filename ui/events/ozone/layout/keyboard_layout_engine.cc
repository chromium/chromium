// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/keyboard_layout_engine.h"

#include "base/check_op.h"
#include "ui/events/event_constants.h"

namespace ui {

bool KeyboardLayoutEngine::LookupNativeKeycode(uint32_t native_keycode,
                                               int event_flags,
                                               DomKey* dom_key,
                                               KeyboardCode* key_code) const {
  return false;
}

std::optional<uint32_t> KeyboardLayoutEngine::LookupKeysym(
    DomCode dom_code,
    int event_flags) const {
  return std::nullopt;
}

uint32_t KeyboardLayoutEngine::GetKeyboardLayoutCount() const {
  return 1;
}

DomKey KeyboardLayoutEngine::LookupForLayout(DomCode dom_code,
                                             uint32_t layout_index) const {
  CHECK_EQ(layout_index, 0u);
  DomKey dom_key;
  KeyboardCode unused_code;
  if (Lookup(dom_code, EF_NONE, &dom_key, &unused_code)) {
    return dom_key;
  }
  return DomKey::NONE;
}

}  // namespace ui
