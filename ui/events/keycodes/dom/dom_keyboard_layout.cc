// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/keycodes/dom/dom_keyboard_layout.h"

#include "base/strings/utf_string_conversion_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

// Mapping from Unicode combining characters to corresponding printable
// character.
const static struct {
  uint16_t combining;
  uint16_t printable;
} kCombiningKeyMapping[] = {
    {0x0300, 0x0060},  // Grave
    {0x0301, 0x0027},  // Acute
    {0x0302, 0x005e},  // Circumflex
    {0x0303, 0x007e},  // Tilde
    {0x0308, 0x00a8},  // Diaeresis
};

DomKeyboardLayout::DomKeyboardLayout() = default;

DomKeyboardLayout::~DomKeyboardLayout() = default;

void DomKeyboardLayout::AddKeyMapping(DomCode code, uint32_t unicode) {
  layout_.emplace(code, unicode);
}

base::flat_map<std::string, std::string> DomKeyboardLayout::GetMap() {
  auto dom_map = base::flat_map<std::string, std::string>();
  for (const ui::DomCode dom_code : ui::kWritingSystemKeyDomCodes) {
    uint16_t unicode = layout_[dom_code];

    // Map combining accents into the corresponding printable character.
    if (unicode >= 0x0300 && unicode <= 0x036f) {
      uint16_t printable = 0;
      for (size_t j = 0; j < std::size(kCombiningKeyMapping); ++j) {
        if (kCombiningKeyMapping[j].combining == unicode) {
          printable = kCombiningKeyMapping[j].printable;
          break;
        }
      }
      unicode = printable;
    }

    if (unicode == 0)
      continue;

    std::string key_str;
    // Special handling for the Japanese BACKQUOTE, which is an IME key used
    // to switch between half-width and full-width mode.
    if (unicode == kHankakuZenkakuPlaceholder) {
      // 半角/全角 = hankaku/zenkaku = halfwidth/fullwidth
      key_str = "\u534a\u89d2/\u5168\u89d2";
    } else {
      size_t len = base::WriteUnicodeCharacter(unicode, &key_str);
      if (len == 0)
        continue;
    }
    dom_map.emplace(KeycodeConverter::DomCodeToCodeString(dom_code), key_str);
  }
  return dom_map;
}

bool DomKeyboardLayout::IsAsciiCapable() {
  uint16_t uniA = layout_[DomCode::US_A];
  return uniA >= 'a' && uniA <= 'z';
}

}  // namespace ui
