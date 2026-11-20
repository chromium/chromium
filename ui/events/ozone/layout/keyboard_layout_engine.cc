// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/keyboard_layout_engine.h"

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

}  // namespace ui
