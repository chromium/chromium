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

const DomCode writing_system_key_domcodes[] = {
    // Keyboard Row E
    DomCode::BACKQUOTE, DomCode::DIGIT1, DomCode::DIGIT2, DomCode::DIGIT3,
    DomCode::DIGIT4, DomCode::DIGIT5, DomCode::DIGIT6, DomCode::DIGIT7,
    DomCode::DIGIT8, DomCode::DIGIT9, DomCode::DIGIT0, DomCode::MINUS,
    DomCode::EQUAL, DomCode::INTL_YEN,

    // Keyboard Row D
    DomCode::US_Q, DomCode::US_W, DomCode::US_E, DomCode::US_R, DomCode::US_T,
    DomCode::US_Y, DomCode::US_U, DomCode::US_I, DomCode::US_O, DomCode::US_P,
    DomCode::BRACKET_LEFT, DomCode::BRACKET_RIGHT, DomCode::BACKSLASH,

    // Keyboard Row C
    DomCode::US_A, DomCode::US_S, DomCode::US_D, DomCode::US_F, DomCode::US_G,
    DomCode::US_H, DomCode::US_J, DomCode::US_K, DomCode::US_L,
    DomCode::SEMICOLON, DomCode::QUOTE,

    // Keyboard Row B
    DomCode::INTL_BACKSLASH, DomCode::US_Z, DomCode::US_X, DomCode::US_C,
    DomCode::US_V, DomCode::US_B, DomCode::US_N, DomCode::US_M, DomCode::COMMA,
    DomCode::PERIOD, DomCode::SLASH, DomCode::INTL_RO,
};

const size_t kWritingSystemKeyDomCodeEntries =
    std::size(writing_system_key_domcodes);

const uint32_t kHankakuZenkakuPlaceholder = 0x89d2;

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
  for (size_t i = 0; i < kWritingSystemKeyDomCodeEntries; ++i) {
    ui::DomCode dom_code = ui::writing_system_key_domcodes[i];
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
