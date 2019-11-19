// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

#include "base/strings/string16.h"
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
  return DomCodeToUsLayoutDomKey(dom_code, flags, out_dom_key, out_key_code);
}

}  // namespace ui
