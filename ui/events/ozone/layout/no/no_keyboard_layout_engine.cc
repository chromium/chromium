// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/no/no_keyboard_layout_engine.h"

namespace ui {

std::string_view NoKeyboardLayoutEngine::GetLayoutName() const {
  return std::string_view();
}

bool NoKeyboardLayoutEngine::CanSetCurrentLayout() const {
  return false;
}

void NoKeyboardLayoutEngine::SetCurrentLayoutByName(
    const std::string& layout_name,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

bool NoKeyboardLayoutEngine::SetCurrentLayoutFromBuffer(const char* keymap_str,
                                                        size_t size) {
  return false;
}

bool NoKeyboardLayoutEngine::UsesISOLevel5Shift() const {
  return false;
}

bool NoKeyboardLayoutEngine::UsesAltGr() const {
  return false;
}

bool NoKeyboardLayoutEngine::Lookup(DomCode dom_code,
                                    int flags,
                                    DomKey* dom_key,
                                    KeyboardCode* key_code) const {
  return false;
}

void NoKeyboardLayoutEngine::SetInitCallbackForTest(base::OnceClosure closure) {
  std::move(closure).Run();
}

}  // namespace ui
