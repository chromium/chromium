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

StubKeyboardLayoutEngine::StubKeyboardLayoutEngine() = default;

StubKeyboardLayoutEngine::~StubKeyboardLayoutEngine() = default;

std::string_view StubKeyboardLayoutEngine::GetLayoutName() const {
  return std::string_view();
}

bool StubKeyboardLayoutEngine::CanSetCurrentLayout() const {
  return false;
}

void StubKeyboardLayoutEngine::SetCurrentLayoutByName(
    const std::string& layout_name,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
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
        bool shift_down = flags & EF_SHIFT_DOWN;
        DomKey key = shift_down ? entry.dom_key_shifted : entry.dom_key;

        // Caps is effect only for alphabet keys.
        bool caps_on = flags & EF_CAPS_LOCK_ON;
        if (caps_on && key.IsCharacter()) {
          uint32_t ch = key.ToCharacter();
          if (ch >= 'a' && ch <= 'z') {
            key = shift_down ? entry.dom_key : entry.dom_key_shifted;
          }
        }

        *out_dom_key = key;
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
