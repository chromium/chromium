// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion.h"

#include <stdint.h>

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/dom_us_layout_data.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

struct Meaning {
  bool defined;
  ui::DomKey::Base dom_key;
  ui::KeyboardCode key_code;
};

void CheckDomCodeToMeaning(const char* label,
                           bool f(ui::DomCode dom_code,
                                  int flags,
                                  ui::DomKey* out_dom_key,
                                  ui::KeyboardCode* out_key_code),
                           ui::DomCode dom_code,
                           int event_flags,
                           const Meaning& result) {
  ui::DomKey result_dom_key = ui::DomKey::NONE;
  ui::KeyboardCode result_key_code = ui::VKEY_UNKNOWN;
  bool success = f(dom_code, event_flags, &result_dom_key, &result_key_code);
  SCOPED_TRACE(base::StringPrintf(
      "%s %s %06X:%04X", label,
      ui::KeycodeConverter::DomCodeToCodeString(dom_code).c_str(),
      static_cast<int>(dom_code), event_flags));
  EXPECT_EQ(result.defined, success);
  if (success) {
    EXPECT_EQ(result.dom_key, result_dom_key)
        << "Expected '"
        << ui::KeycodeConverter::DomKeyToKeyString(result.dom_key)
        << "' Actual '"
        << ui::KeycodeConverter::DomKeyToKeyString(result_dom_key) << "'"
        << " when testing DomCode '"
        << ui::KeycodeConverter::DomCodeToCodeString(dom_code) << "' ["
        << static_cast<int>(dom_code) << "]";

    EXPECT_EQ(result.key_code, result_key_code);
  } else {
    // Should not have touched output parameters.
    EXPECT_EQ(ui::DomKey::NONE, result_dom_key);
    EXPECT_EQ(ui::VKEY_UNKNOWN, result_key_code);
  }
}

TEST(KeyboardCodeConversion, UsLayout) {
  static const struct {
    ui::DomCode dom_code;
    Meaning normal;
    Meaning shift;
  } kPrintableUsLayout[] = {
      {ui::DomCode::US_A,
       {true, ui::DomKey::Constant<'a'>::Character, ui::VKEY_A},
       {true, ui::DomKey::Constant<'A'>::Character, ui::VKEY_A}},
      {ui::DomCode::US_B,
       {true, ui::DomKey::Constant<'b'>::Character, ui::VKEY_B},
       {true, ui::DomKey::Constant<'B'>::Character, ui::VKEY_B}},
      {ui::DomCode::US_C,
       {true, ui::DomKey::Constant<'c'>::Character, ui::VKEY_C},
       {true, ui::DomKey::Constant<'C'>::Character, ui::VKEY_C}},
      {ui::DomCode::US_D,
       {true, ui::DomKey::Constant<'d'>::Character, ui::VKEY_D},
       {true, ui::DomKey::Constant<'D'>::Character, ui::VKEY_D}},
      {ui::DomCode::US_E,
       {true, ui::DomKey::Constant<'e'>::Character, ui::VKEY_E},
       {true, ui::DomKey::Constant<'E'>::Character, ui::VKEY_E}},
      {ui::DomCode::US_F,
       {true, ui::DomKey::Constant<'f'>::Character, ui::VKEY_F},
       {true, ui::DomKey::Constant<'F'>::Character, ui::VKEY_F}},
      {ui::DomCode::US_G,
       {true, ui::DomKey::Constant<'g'>::Character, ui::VKEY_G},
       {true, ui::DomKey::Constant<'G'>::Character, ui::VKEY_G}},
      {ui::DomCode::US_H,
       {true, ui::DomKey::Constant<'h'>::Character, ui::VKEY_H},
       {true, ui::DomKey::Constant<'H'>::Character, ui::VKEY_H}},
      {ui::DomCode::US_I,
       {true, ui::DomKey::Constant<'i'>::Character, ui::VKEY_I},
       {true, ui::DomKey::Constant<'I'>::Character, ui::VKEY_I}},
      {ui::DomCode::US_J,
       {true, ui::DomKey::Constant<'j'>::Character, ui::VKEY_J},
       {true, ui::DomKey::Constant<'J'>::Character, ui::VKEY_J}},
      {ui::DomCode::US_K,
       {true, ui::DomKey::Constant<'k'>::Character, ui::VKEY_K},
       {true, ui::DomKey::Constant<'K'>::Character, ui::VKEY_K}},
      {ui::DomCode::US_L,
       {true, ui::DomKey::Constant<'l'>::Character, ui::VKEY_L},
       {true, ui::DomKey::Constant<'L'>::Character, ui::VKEY_L}},
      {ui::DomCode::US_M,
       {true, ui::DomKey::Constant<'m'>::Character, ui::VKEY_M},
       {true, ui::DomKey::Constant<'M'>::Character, ui::VKEY_M}},
      {ui::DomCode::US_N,
       {true, ui::DomKey::Constant<'n'>::Character, ui::VKEY_N},
       {true, ui::DomKey::Constant<'N'>::Character, ui::VKEY_N}},
      {ui::DomCode::US_O,
       {true, ui::DomKey::Constant<'o'>::Character, ui::VKEY_O},
       {true, ui::DomKey::Constant<'O'>::Character, ui::VKEY_O}},
      {ui::DomCode::US_P,
       {true, ui::DomKey::Constant<'p'>::Character, ui::VKEY_P},
       {true, ui::DomKey::Constant<'P'>::Character, ui::VKEY_P}},
      {ui::DomCode::US_Q,
       {true, ui::DomKey::Constant<'q'>::Character, ui::VKEY_Q},
       {true, ui::DomKey::Constant<'Q'>::Character, ui::VKEY_Q}},
      {ui::DomCode::US_R,
       {true, ui::DomKey::Constant<'r'>::Character, ui::VKEY_R},
       {true, ui::DomKey::Constant<'R'>::Character, ui::VKEY_R}},
      {ui::DomCode::US_S,
       {true, ui::DomKey::Constant<'s'>::Character, ui::VKEY_S},
       {true, ui::DomKey::Constant<'S'>::Character, ui::VKEY_S}},
      {ui::DomCode::US_T,
       {true, ui::DomKey::Constant<'t'>::Character, ui::VKEY_T},
       {true, ui::DomKey::Constant<'T'>::Character, ui::VKEY_T}},
      {ui::DomCode::US_U,
       {true, ui::DomKey::Constant<'u'>::Character, ui::VKEY_U},
       {true, ui::DomKey::Constant<'U'>::Character, ui::VKEY_U}},
      {ui::DomCode::US_V,
       {true, ui::DomKey::Constant<'v'>::Character, ui::VKEY_V},
       {true, ui::DomKey::Constant<'V'>::Character, ui::VKEY_V}},
      {ui::DomCode::US_W,
       {true, ui::DomKey::Constant<'w'>::Character, ui::VKEY_W},
       {true, ui::DomKey::Constant<'W'>::Character, ui::VKEY_W}},
      {ui::DomCode::US_X,
       {true, ui::DomKey::Constant<'x'>::Character, ui::VKEY_X},
       {true, ui::DomKey::Constant<'X'>::Character, ui::VKEY_X}},
      {ui::DomCode::US_Y,
       {true, ui::DomKey::Constant<'y'>::Character, ui::VKEY_Y},
       {true, ui::DomKey::Constant<'Y'>::Character, ui::VKEY_Y}},
      {ui::DomCode::US_Z,
       {true, ui::DomKey::Constant<'z'>::Character, ui::VKEY_Z},
       {true, ui::DomKey::Constant<'Z'>::Character, ui::VKEY_Z}},
      {ui::DomCode::DIGIT1,
       {true, ui::DomKey::Constant<'1'>::Character, ui::VKEY_1},
       {true, ui::DomKey::Constant<'!'>::Character, ui::VKEY_1}},
      {ui::DomCode::DIGIT2,
       {true, ui::DomKey::Constant<'2'>::Character, ui::VKEY_2},
       {true, ui::DomKey::Constant<'@'>::Character, ui::VKEY_2}},
      {ui::DomCode::DIGIT3,
       {true, ui::DomKey::Constant<'3'>::Character, ui::VKEY_3},
       {true, ui::DomKey::Constant<'#'>::Character, ui::VKEY_3}},
      {ui::DomCode::DIGIT4,
       {true, ui::DomKey::Constant<'4'>::Character, ui::VKEY_4},
       {true, ui::DomKey::Constant<'$'>::Character, ui::VKEY_4}},
      {ui::DomCode::DIGIT5,
       {true, ui::DomKey::Constant<'5'>::Character, ui::VKEY_5},
       {true, ui::DomKey::Constant<'%'>::Character, ui::VKEY_5}},
      {ui::DomCode::DIGIT6,
       {true, ui::DomKey::Constant<'6'>::Character, ui::VKEY_6},
       {true, ui::DomKey::Constant<'^'>::Character, ui::VKEY_6}},
      {ui::DomCode::DIGIT7,
       {true, ui::DomKey::Constant<'7'>::Character, ui::VKEY_7},
       {true, ui::DomKey::Constant<'&'>::Character, ui::VKEY_7}},
      {ui::DomCode::DIGIT8,
       {true, ui::DomKey::Constant<'8'>::Character, ui::VKEY_8},
       {true, ui::DomKey::Constant<'*'>::Character, ui::VKEY_8}},
      {ui::DomCode::DIGIT9,
       {true, ui::DomKey::Constant<'9'>::Character, ui::VKEY_9},
       {true, ui::DomKey::Constant<'('>::Character, ui::VKEY_9}},
      {ui::DomCode::DIGIT0,
       {true, ui::DomKey::Constant<'0'>::Character, ui::VKEY_0},
       {true, ui::DomKey::Constant<')'>::Character, ui::VKEY_0}},
      {ui::DomCode::SPACE,
       {true, ui::DomKey::Constant<' '>::Character, ui::VKEY_SPACE},
       {true, ui::DomKey::Constant<' '>::Character, ui::VKEY_SPACE}},
      {ui::DomCode::MINUS,
       {true, ui::DomKey::Constant<'-'>::Character, ui::VKEY_OEM_MINUS},
       {true, ui::DomKey::Constant<'_'>::Character, ui::VKEY_OEM_MINUS}},
      {ui::DomCode::EQUAL,
       {true, ui::DomKey::Constant<'='>::Character, ui::VKEY_OEM_PLUS},
       {true, ui::DomKey::Constant<'+'>::Character, ui::VKEY_OEM_PLUS}},
      {ui::DomCode::BRACKET_LEFT,
       {true, ui::DomKey::Constant<'['>::Character, ui::VKEY_OEM_4},
       {true, ui::DomKey::Constant<'{'>::Character, ui::VKEY_OEM_4}},
      {ui::DomCode::BRACKET_RIGHT,
       {true, ui::DomKey::Constant<']'>::Character, ui::VKEY_OEM_6},
       {true, ui::DomKey::Constant<'}'>::Character, ui::VKEY_OEM_6}},
      {ui::DomCode::BACKSLASH,
       {true, ui::DomKey::Constant<'\\'>::Character, ui::VKEY_OEM_5},
       {true, ui::DomKey::Constant<'|'>::Character, ui::VKEY_OEM_5}},
      {ui::DomCode::SEMICOLON,
       {true, ui::DomKey::Constant<';'>::Character, ui::VKEY_OEM_1},
       {true, ui::DomKey::Constant<':'>::Character, ui::VKEY_OEM_1}},
      {ui::DomCode::QUOTE,
       {true, ui::DomKey::Constant<'\''>::Character, ui::VKEY_OEM_7},
       {true, ui::DomKey::Constant<'"'>::Character, ui::VKEY_OEM_7}},
      {ui::DomCode::BACKQUOTE,
       {true, ui::DomKey::Constant<'`'>::Character, ui::VKEY_OEM_3},
       {true, ui::DomKey::Constant<'~'>::Character, ui::VKEY_OEM_3}},
      {ui::DomCode::COMMA,
       {true, ui::DomKey::Constant<','>::Character, ui::VKEY_OEM_COMMA},
       {true, ui::DomKey::Constant<'<'>::Character, ui::VKEY_OEM_COMMA}},
      {ui::DomCode::PERIOD,
       {true, ui::DomKey::Constant<'.'>::Character, ui::VKEY_OEM_PERIOD},
       {true, ui::DomKey::Constant<'>'>::Character, ui::VKEY_OEM_PERIOD}},
      {ui::DomCode::SLASH,
       {true, ui::DomKey::Constant<'/'>::Character, ui::VKEY_OEM_2},
       {true, ui::DomKey::Constant<'?'>::Character, ui::VKEY_OEM_2}},
      {ui::DomCode::INTL_BACKSLASH,
       {true, ui::DomKey::Constant<'<'>::Character, ui::VKEY_OEM_102},
       {true, ui::DomKey::Constant<'>'>::Character, ui::VKEY_OEM_102}},
      {ui::DomCode::INTL_YEN,
       {true, ui::DomKey::Constant<0x00A5>::Character, ui::VKEY_OEM_5},
       {true, ui::DomKey::Constant<'|'>::Character, ui::VKEY_OEM_5}},
      {ui::DomCode::NUMPAD_DIVIDE,
       {true, ui::DomKey::Constant<'/'>::Character, ui::VKEY_DIVIDE},
       {true, ui::DomKey::Constant<'/'>::Character, ui::VKEY_DIVIDE}},
      {ui::DomCode::NUMPAD_MULTIPLY,
       {true, ui::DomKey::Constant<'*'>::Character, ui::VKEY_MULTIPLY},
       {true, ui::DomKey::Constant<'*'>::Character, ui::VKEY_MULTIPLY}},
      {ui::DomCode::NUMPAD_SUBTRACT,
       {true, ui::DomKey::Constant<'-'>::Character, ui::VKEY_SUBTRACT},
       {true, ui::DomKey::Constant<'-'>::Character, ui::VKEY_SUBTRACT}},
      {ui::DomCode::NUMPAD_ADD,
       {true, ui::DomKey::Constant<'+'>::Character, ui::VKEY_ADD},
       {true, ui::DomKey::Constant<'+'>::Character, ui::VKEY_ADD}},
      {ui::DomCode::NUMPAD1,
       {true, ui::DomKey::Constant<'1'>::Character, ui::VKEY_1},
       {true, ui::DomKey::Constant<'1'>::Character, ui::VKEY_1}},
      {ui::DomCode::NUMPAD2,
       {true, ui::DomKey::Constant<'2'>::Character, ui::VKEY_2},
       {true, ui::DomKey::Constant<'2'>::Character, ui::VKEY_2}},
      {ui::DomCode::NUMPAD3,
       {true, ui::DomKey::Constant<'3'>::Character, ui::VKEY_3},
       {true, ui::DomKey::Constant<'3'>::Character, ui::VKEY_3}},
      {ui::DomCode::NUMPAD4,
       {true, ui::DomKey::Constant<'4'>::Character, ui::VKEY_4},
       {true, ui::DomKey::Constant<'4'>::Character, ui::VKEY_4}},
      {ui::DomCode::NUMPAD5,
       {true, ui::DomKey::Constant<'5'>::Character, ui::VKEY_5},
       {true, ui::DomKey::Constant<'5'>::Character, ui::VKEY_5}},
      {ui::DomCode::NUMPAD6,
       {true, ui::DomKey::Constant<'6'>::Character, ui::VKEY_6},
       {true, ui::DomKey::Constant<'6'>::Character, ui::VKEY_6}},
      {ui::DomCode::NUMPAD7,
       {true, ui::DomKey::Constant<'7'>::Character, ui::VKEY_7},
       {true, ui::DomKey::Constant<'7'>::Character, ui::VKEY_7}},
      {ui::DomCode::NUMPAD8,
       {true, ui::DomKey::Constant<'8'>::Character, ui::VKEY_8},
       {true, ui::DomKey::Constant<'8'>::Character, ui::VKEY_8}},
      {ui::DomCode::NUMPAD9,
       {true, ui::DomKey::Constant<'9'>::Character, ui::VKEY_9},
       {true, ui::DomKey::Constant<'9'>::Character, ui::VKEY_9}},
      {ui::DomCode::NUMPAD0,
       {true, ui::DomKey::Constant<'0'>::Character, ui::VKEY_0},
       {true, ui::DomKey::Constant<'0'>::Character, ui::VKEY_0}},
      {ui::DomCode::NUMPAD_DECIMAL,
       {true, ui::DomKey::Constant<'.'>::Character, ui::VKEY_DECIMAL},
       {true, ui::DomKey::Constant<'.'>::Character, ui::VKEY_DECIMAL}},
      {ui::DomCode::NUMPAD_EQUAL,
       {true, ui::DomKey::Constant<'='>::Character, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::Constant<'='>::Character, ui::VKEY_UNKNOWN}},
      {ui::DomCode::NUMPAD_COMMA,
       {true, ui::DomKey::Constant<','>::Character, ui::VKEY_OEM_COMMA},
       {true, ui::DomKey::Constant<','>::Character, ui::VKEY_OEM_COMMA}},
      {ui::DomCode::NUMPAD_PAREN_LEFT,
       {true, ui::DomKey::Constant<'('>::Character, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::Constant<'('>::Character, ui::VKEY_UNKNOWN}},
      {ui::DomCode::NUMPAD_PAREN_RIGHT,
       {true, ui::DomKey::Constant<')'>::Character, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::Constant<')'>::Character, ui::VKEY_UNKNOWN}},
      {ui::DomCode::NUMPAD_SIGN_CHANGE,
       {true, ui::DomKey::Constant<0xB1>::Character, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::Constant<0xB1>::Character, ui::VKEY_UNKNOWN}},
  };

  for (const auto& it : kPrintableUsLayout) {
    CheckDomCodeToMeaning("p_us_n", ui::DomCodeToUsLayoutDomKey, it.dom_code,
                          ui::EF_NONE, it.normal);
    CheckDomCodeToMeaning("p_us_s", ui::DomCodeToUsLayoutDomKey, it.dom_code,
                          ui::EF_SHIFT_DOWN, it.shift);
    CheckDomCodeToMeaning("p_us_a", ui::DomCodeToUsLayoutDomKey, it.dom_code,
                          ui::EF_ALTGR_DOWN, it.normal);
    CheckDomCodeToMeaning("p_us_a", ui::DomCodeToUsLayoutDomKey, it.dom_code,
                          ui::EF_ALTGR_DOWN|ui::EF_SHIFT_DOWN, it.shift);
  }
}

TEST(KeyboardCodeConversion, Tables) {
  // Verify that kDomCodeToKeyboardCodeMap is ordered by DomCode value.
  uint32_t previous = 0;
  for (const auto& it : ui::kDomCodeToKeyboardCodeMap) {
    uint32_t current = static_cast<uint32_t>(it.dom_code);
    EXPECT_LT(previous, current)
        << "kDomCodeToKeyboardCodeMap is not ordered by DomCode\n";
    previous = current;
  }
}

}  // namespace
