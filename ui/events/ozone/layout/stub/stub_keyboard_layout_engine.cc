// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

#include <string>

#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace ui {

StubKeyboardLayoutEngine::StubKeyboardLayoutEngine() {
}

StubKeyboardLayoutEngine::~StubKeyboardLayoutEngine() {
}

bool StubKeyboardLayoutEngine::CanSetCurrentLayout() const {
  return false;
}

bool StubKeyboardLayoutEngine::SetCurrentLayoutByName(
    const std::string& layout_name) {
  return false;
}

bool StubKeyboardLayoutEngine::SetCurrentLayoutFromBuffer(
    const char* keymap_str,
    size_t size) {
  return false;
}

bool StubKeyboardLayoutEngine::UsesISOLevel5Shift() const {
  return false;
}

bool StubKeyboardLayoutEngine::UsesAltGr() const {
  return true;
}

bool StubKeyboardLayoutEngine::Lookup(DomCode dom_code,
                                      int flags,
                                      DomKey* out_dom_key,
                                      KeyboardCode* out_key_code) const {
  if (!custom_lookup_.empty()) {
    for (const auto& entry : custom_lookup_) {
      if (entry.dom_code == dom_code) {
        int shift_down = ((flags & EF_SHIFT_DOWN) == EF_SHIFT_DOWN);
        char16_t ch;
        if (shift_down) {
          ch = entry.character_shifted;
        } else {
          ch = entry.character;
        }
        if ((flags & EF_CAPS_LOCK_ON) == EF_CAPS_LOCK_ON) {
          if ((ch >= 'a') && (ch <= 'z')) {
            if (shift_down) {
              ch = entry.character;
            } else {
              ch = entry.character_shifted;
            }
          }
        }
        *out_dom_key = DomKey::FromCharacter(ch);
        *out_key_code = entry.key_code;
        return true;
      }
    }
  }

  return DomCodeToUsLayoutDomKey(dom_code, flags, out_dom_key, out_key_code);
}

void StubKeyboardLayoutEngine::SetInitCallbackForTest(
    base::OnceClosure closure) {
  std::move(closure).Run();
}

void StubKeyboardLayoutEngine::SetCustomLookupTableForTesting(
    const std::vector<CustomLookupEntry>& table) {
  custom_lookup_ = table;
}

}  // namespace ui
