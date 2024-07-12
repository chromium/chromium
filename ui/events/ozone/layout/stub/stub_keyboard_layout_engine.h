// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_STUB_STUB_KEYBOARD_LAYOUT_ENGINE_H_
#define UI_EVENTS_OZONE_LAYOUT_STUB_STUB_KEYBOARD_LAYOUT_ENGINE_H_

#include <vector>

#include "base/component_export.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"

namespace ui {

class COMPONENT_EXPORT(EVENTS_OZONE_LAYOUT) StubKeyboardLayoutEngine
    : public KeyboardLayoutEngine {
 public:
  struct CustomLookupEntry {
    ui::DomCode dom_code;
    ui::DomKey dom_key;
    ui::DomKey dom_key_shifted;
    ui::KeyboardCode key_code;
  };

  StubKeyboardLayoutEngine();
  ~StubKeyboardLayoutEngine() override;

  // KeyboardLayoutEngineOzone:
  std::string_view GetLayoutName() const override;
  bool CanSetCurrentLayout() const override;
  void SetCurrentLayoutByName(const std::string& layout_name,
                              base::OnceCallback<void(bool)> callback) override;
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
