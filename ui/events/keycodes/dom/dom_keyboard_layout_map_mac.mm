// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>

#include <cstdint>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/check_op.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_map_base.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

namespace ui {

namespace {

class DomKeyboardLayoutMapMac : public ui::DomKeyboardLayoutMapBase {
 public:
  DomKeyboardLayoutMapMac();

  DomKeyboardLayoutMapMac(const DomKeyboardLayoutMapMac&) = delete;
  DomKeyboardLayoutMapMac& operator=(const DomKeyboardLayoutMapMac&) = delete;

  ~DomKeyboardLayoutMapMac() override;

  // ui::DomKeyboardLayoutMapBase implementation.
  uint32_t GetKeyboardLayoutCount() override;
  ui::DomKey GetDomKeyFromDomCodeForLayout(
      ui::DomCode dom_code,
      uint32_t keyboard_layout_index) override;
};

DomKeyboardLayoutMapMac::DomKeyboardLayoutMapMac() = default;

DomKeyboardLayoutMapMac::~DomKeyboardLayoutMapMac() = default;

uint32_t DomKeyboardLayoutMapMac::GetKeyboardLayoutCount() {
  return 1;
}

ui::DomKey DomKeyboardLayoutMapMac::GetDomKeyFromDomCodeForLayout(
    ui::DomCode dom_code,
    uint32_t keyboard_layout_index) {
  DCHECK_NE(dom_code, ui::DomCode::NONE);
  DCHECK_EQ(keyboard_layout_index, 0U);

  UInt32 dead_key_state = 0;
  uint16_t key_code = ui::KeycodeConverter::DomCodeToNativeKeycode(dom_code);
  base::apple::ScopedCFTypeRef<TISInputSourceRef> input_source(
      TISCopyCurrentASCIICapableKeyboardLayoutInputSource());
  UniChar char_value = ui::TranslatedUnicodeCharFromKeyCode(
      input_source.get(), key_code, kUCKeyActionDisplay, 0, LMGetKbdType(),
      &dead_key_state);

  if (!char_value)
    return ui::DomKey::NONE;

  if (dead_key_state)
    return DomKey::DeadKeyFromCombiningCharacter(char_value);

  return DomKey::FromCharacter(char_value);
}

}  // namespace

// static
base::flat_map<std::string, std::string> GenerateDomKeyboardLayoutMap() {
  return DomKeyboardLayoutMapMac().Generate();
}

}  // namespace ui
