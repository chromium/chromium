// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_STUB_STUB_KEYBOARD_LAYOUT_ENGINE_H_
#define UI_EVENTS_OZONE_LAYOUT_STUB_STUB_KEYBOARD_LAYOUT_ENGINE_H_

#include <vector>

#include "base/component_export.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"

namespace ui {

class COMPONENT_EXPORT(EVENTS_OZONE_LAYOUT) StubKeyboardLayoutEngine
    : public KeyboardLayoutEngine {
 public:
  struct CustomLookupEntry {
    ui::DomCode dom_code;
    char16_t character;
    char16_t character_shifted;
    ui::KeyboardCode key_code;
  };

  StubKeyboardLayoutEngine();
  ~StubKeyboardLayoutEngine() override;

  // KeyboardLayoutEngineOzone:
  bool CanSetCurrentLayout() const override;
  bool SetCurrentLayoutByName(const std::string& layout_name) override;
  bool SetCurrentLayoutFromBuffer(const char* keymap_string,
                                  size_t size) override;
  bool UsesISOLevel5Shift() const override;
  bool UsesAltGr() const override;
  bool Lookup(DomCode dom_code,
              int flags,
              DomKey* dom_key,
              KeyboardCode* key_code) const override;
  void SetInitCallbackForTest(base::OnceClosure closure) override;

  void SetCustomLookupTableForTesting(
      const std::vector<CustomLookupEntry>& table);

 private:
  std::vector<CustomLookupEntry> custom_lookup_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_STUB_STUB_KEYBOARD_LAYOUT_ENGINE_H_
