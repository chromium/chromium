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

const Meaning kUndefined = {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN};

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

TEST(KeyboardCodeConversion, ControlCharacters) {
  // The codes in this table are handled by |DomCodeToControlCharacter()|.
  static const struct {
    ui::DomCode dom_code;
    Meaning control_character;
    Meaning control_key;
  } kControlCharacters[] = {
      {ui::DomCode::US_A,
       {true, ui::DomKey::Constant<0x01>::Character, ui::VKEY_A},
       {true, ui::DomKey::Constant<'a'>::Character, ui::VKEY_A}},
      {ui::DomCode::US_B,
       {true, ui::DomKey::Constant<0x02>::Character, ui::VKEY_B},
       {true, ui::DomKey::Constant<'b'>::Character, ui::VKEY_B}},
      {ui::DomCode::US_C,
       {true, ui::DomKey::Constant<0x03>::Character, ui::VKEY_C},
       {true, ui::DomKey::Constant<'c'>::Character, ui::VKEY_C}},
      {ui::DomCode::US_D,
       {true, ui::DomKey::Constant<0x04>::Character, ui::VKEY_D},
       {true, ui::DomKey::Constant<'d'>::Character, ui::VKEY_D}},
      {ui::DomCode::US_E,
       {true, ui::DomKey::Constant<0x05>::Character, ui::VKEY_E},
       {true, ui::DomKey::Constant<'e'>::Character, ui::VKEY_E}},
      {ui::DomCode::US_F,
       {true, ui::DomKey::Constant<0x06>::Character, ui::VKEY_F},
       {true, ui::DomKey::Constant<'f'>::Character, ui::VKEY_F}},
      {ui::DomCode::US_G,
       {true, ui::DomKey::Constant<0x07>::Character, ui::VKEY_G},
       {true, ui::DomKey::Constant<'g'>::Character, ui::VKEY_G}},
      {ui::DomCode::US_H,
       {true, ui::DomKey::BACKSPACE, ui::VKEY_BACK},
       {true, ui::DomKey::Constant<'h'>::Character, ui::VKEY_H}},
      {ui::DomCode::US_I,
       {true, ui::DomKey::TAB, ui::VKEY_TAB},
       {true, ui::DomKey::Constant<'i'>::Character, ui::VKEY_I}},
      {ui::DomCode::US_J,
       {true, ui::DomKey::Constant<0x0A>::Character, ui::VKEY_J},
       {true, ui::DomKey::Constant<'j'>::Character, ui::VKEY_J}},
      {ui::DomCode::US_K,
       {true, ui::DomKey::Constant<0x0B>::Character, ui::VKEY_K},
       {true, ui::DomKey::Constant<'k'>::Character, ui::VKEY_K}},
      {ui::DomCode::US_L,
       {true, ui::DomKey::Constant<0x0C>::Character, ui::VKEY_L},
       {true, ui::DomKey::Constant<'l'>::Character, ui::VKEY_L}},
      {ui::DomCode::US_M,
       {true, ui::DomKey::ENTER, ui::VKEY_RETURN},
       {true, ui::DomKey::Constant<'m'>::Character, ui::VKEY_M}},
      {ui::DomCode::US_N,
       {true, ui::DomKey::Constant<0x0E>::Character, ui::VKEY_N},
       {true, ui::DomKey::Constant<'n'>::Character, ui::VKEY_N}},
      {ui::DomCode::US_O,
       {true, ui::DomKey::Constant<0x0F>::Character, ui::VKEY_O},
       {true, ui::DomKey::Constant<'o'>::Character, ui::VKEY_O}},
      {ui::DomCode::US_P,
       {true, ui::DomKey::Constant<0x10>::Character, ui::VKEY_P},
       {true, ui::DomKey::Constant<'p'>::Character, ui::VKEY_P}},
      {ui::DomCode::US_Q,
       {true, ui::DomKey::Constant<0x11>::Character, ui::VKEY_Q},
       {true, ui::DomKey::Constant<'q'>::Character, ui::VKEY_Q}},
      {ui::DomCode::US_R,
       {true, ui::DomKey::Constant<0x12>::Character, ui::VKEY_R},
       {true, ui::DomKey::Constant<'r'>::Character, ui::VKEY_R}},
      {ui::DomCode::US_S,
       {true, ui::DomKey::Constant<0x13>::Character, ui::VKEY_S},
       {true, ui::DomKey::Constant<'s'>::Character, ui::VKEY_S}},
      {ui::DomCode::US_T,
       {true, ui::DomKey::Constant<0x14>::Character, ui::VKEY_T},
       {true, ui::DomKey::Constant<'t'>::Character, ui::VKEY_T}},
      {ui::DomCode::US_U,
       {true, ui::DomKey::Constant<0x15>::Character, ui::VKEY_U},
       {true, ui::DomKey::Constant<'u'>::Character, ui::VKEY_U}},
      {ui::DomCode::US_V,
       {true, ui::DomKey::Constant<0x16>::Character, ui::VKEY_V},
       {true, ui::DomKey::Constant<'v'>::Character, ui::VKEY_V}},
      {ui::DomCode::US_W,
       {true, ui::DomKey::Constant<0x17>::Character, ui::VKEY_W},
       {true, ui::DomKey::Constant<'w'>::Character, ui::VKEY_W}},
      {ui::DomCode::US_X,
       {true, ui::DomKey::Constant<0x18>::Character, ui::VKEY_X},
       {true, ui::DomKey::Constant<'x'>::Character, ui::VKEY_X}},
      {ui::DomCode::US_Y,
       {true, ui::DomKey::Constant<0x19>::Character, ui::VKEY_Y},
       {true, ui::DomKey::Constant<'y'>::Character, ui::VKEY_Y}},
      {ui::DomCode::US_Z,
       {true, ui::DomKey::Constant<0x1A>::Character, ui::VKEY_Z},
       {true, ui::DomKey::Constant<'z'>::Character, ui::VKEY_Z}},
  };
  for (const auto& it : kControlCharacters) {
    // Verify |DomCodeToControlCharacter()|.
    CheckDomCodeToMeaning("c_cc_n", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_NONE, kUndefined);
    CheckDomCodeToMeaning("c_cc_c", ui::DomCodeToControlCharacter, it.dom_code,
                           ui::EF_CONTROL_DOWN, it.control_character);
    CheckDomCodeToMeaning("c_cc_cs", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                          it.control_character);
    // Verify |DomCodeToUsLayoutDomKey()|.
    CheckDomCodeToMeaning("c_us_c", ui::DomCodeToUsLayoutDomKey, it.dom_code,
                          ui::EF_CONTROL_DOWN, it.control_key);
  }

  // The codes in this table are sensitive to the Shift state, so they are
  // handled differently by |DomCodeToControlCharacter()|, which returns false
  // for unknown combinations, vs |DomCodeToUsLayoutDomKey()|, which returns
  // true with DomKey::UNIDENTIFIED.
  static const struct {
    ui::DomCode dom_code;
    Meaning cc_control;
    Meaning cc_control_shift;
    Meaning us_control;
    Meaning us_control_shift;
  } kShiftControlCharacters[] = {
      {ui::DomCode::DIGIT2,
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::Constant<0>::Character, ui::VKEY_2},
       {true, ui::DomKey::Constant<'2'>::Character, ui::VKEY_2},
       {true, ui::DomKey::Constant<'@'>::Character, ui::VKEY_2}},
      {ui::DomCode::DIGIT6,
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::Constant<0x1E>::Character, ui::VKEY_6},
       {true, ui::DomKey::Constant<'6'>::Character, ui::VKEY_6},
       {true, ui::DomKey::Constant<'^'>::Character, ui::VKEY_6}},
      {ui::DomCode::MINUS,
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::Constant<0x1F>::Character, ui::VKEY_OEM_MINUS},
       {true, ui::DomKey::Constant<'-'>::Character, ui::VKEY_OEM_MINUS},
       {true, ui::DomKey::Constant<'_'>::Character, ui::VKEY_OEM_MINUS}},
      {ui::DomCode::ENTER,
       {true, ui::DomKey::Constant<0x0A>::Character, ui::VKEY_RETURN},
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::ENTER, ui::VKEY_RETURN},
       {true, ui::DomKey::ENTER, ui::VKEY_RETURN}},
      {ui::DomCode::BRACKET_LEFT,
       {true, ui::DomKey::ESCAPE, ui::VKEY_OEM_4},
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::Constant<'['>::Character, ui::VKEY_OEM_4},
       {true, ui::DomKey::Constant<'{'>::Character, ui::VKEY_OEM_4}},
      {ui::DomCode::BACKSLASH,
       {true, ui::DomKey::Constant<0x1C>::Character, ui::VKEY_OEM_5},
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::Constant<'\\'>::Character, ui::VKEY_OEM_5},
       {true, ui::DomKey::Constant<'|'>::Character, ui::VKEY_OEM_5}},
      {ui::DomCode::BRACKET_RIGHT,
       {true, ui::DomKey::Constant<0x1D>::Character, ui::VKEY_OEM_6},
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::Constant<']'>::Character, ui::VKEY_OEM_6},
       {true, ui::DomKey::Constant<'}'>::Character, ui::VKEY_OEM_6}},
  };
  for (const auto& it : kShiftControlCharacters) {
    // Verify |DomCodeToControlCharacter()|.
    CheckDomCodeToMeaning("s_cc_n", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_NONE, kUndefined);
    CheckDomCodeToMeaning("s_cc_c", ui::DomCodeToControlCharacter, it.dom_code,
                           ui::EF_CONTROL_DOWN, it.cc_control);
    CheckDomCodeToMeaning("s_cc_cs", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                          it.cc_control_shift);
    // Verify |DomCodeToUsLayoutDomKey()|.
    CheckDomCodeToMeaning("s_us_c", ui::DomCodeToUsLayoutDomKey, it.dom_code,
                          ui::EF_CONTROL_DOWN, it.us_control);
    CheckDomCodeToMeaning("s_us_cs", ui::DomCodeToUsLayoutDomKey, it.dom_code,
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                          it.us_control_shift);
  }

  // These codes are not handled by |DomCodeToControlCharacter()| directly.
  static const struct {
    ui::DomCode dom_code;
    Meaning normal;
    Meaning shift;
  } kNonControlCharacters[] = {
      // Modifiers are handled by |DomCodeToUsLayoutDomKey()| without regard
      // to whether Control is down.
      {ui::DomCode::CONTROL_LEFT,
       {true, ui::DomKey::CONTROL, ui::VKEY_CONTROL},
       {true, ui::DomKey::CONTROL, ui::VKEY_CONTROL}},
      {ui::DomCode::CONTROL_RIGHT,
       {true, ui::DomKey::CONTROL, ui::VKEY_CONTROL},
       {true, ui::DomKey::CONTROL, ui::VKEY_CONTROL}},
      {ui::DomCode::SHIFT_LEFT,
       {true, ui::DomKey::SHIFT, ui::VKEY_SHIFT},
       {true, ui::DomKey::SHIFT, ui::VKEY_SHIFT}},
      {ui::DomCode::SHIFT_RIGHT,
       {true, ui::DomKey::SHIFT, ui::VKEY_SHIFT},
       {true, ui::DomKey::SHIFT, ui::VKEY_SHIFT}},
      {ui::DomCode::ALT_LEFT,
       {true, ui::DomKey::ALT, ui::VKEY_MENU},
       {true, ui::DomKey::ALT, ui::VKEY_MENU}},
      {ui::DomCode::ALT_RIGHT,
       {true, ui::DomKey::ALT, ui::VKEY_MENU},
       {true, ui::DomKey::ALT, ui::VKEY_MENU}},
      {ui::DomCode::META_LEFT,
       {true, ui::DomKey::META, ui::VKEY_LWIN},
       {true, ui::DomKey::META, ui::VKEY_LWIN}},
      {ui::DomCode::META_RIGHT,
       {true, ui::DomKey::META, ui::VKEY_LWIN},
       {true, ui::DomKey::META, ui::VKEY_LWIN}},
      {ui::DomCode::DIGIT1,
       {true, ui::DomKey::Constant<'1'>::Character, ui::VKEY_1},
       {true, ui::DomKey::Constant<'!'>::Character, ui::VKEY_1}},
      {ui::DomCode::EQUAL,
       {true, ui::DomKey::Constant<'='>::Character, ui::VKEY_OEM_PLUS},
       {true, ui::DomKey::Constant<'+'>::Character, ui::VKEY_OEM_PLUS}},
      {ui::DomCode::TAB,
       {true, ui::DomKey::TAB, ui::VKEY_TAB},
       {true, ui::DomKey::TAB, ui::VKEY_TAB}},
      {ui::DomCode::F1,
       {true, ui::DomKey::F1, ui::VKEY_F1},
       {true, ui::DomKey::F1, ui::VKEY_F1}},
      {ui::DomCode::VOLUME_UP,
       {true, ui::DomKey::AUDIO_VOLUME_UP, ui::VKEY_VOLUME_UP},
       {true, ui::DomKey::AUDIO_VOLUME_UP, ui::VKEY_VOLUME_UP}},
      {ui::DomCode::PRINT,
       {true, ui::DomKey::PRINT, ui::VKEY_PRINT},
       {true, ui::DomKey::PRINT, ui::VKEY_PRINT}},
      {ui::DomCode::PRINT_SCREEN,
       {true, ui::DomKey::PRINT_SCREEN, ui::VKEY_SNAPSHOT},
       {true, ui::DomKey::PRINT_SCREEN, ui::VKEY_SNAPSHOT}},
  };
  for (const auto& it : kNonControlCharacters) {
    // Verify |DomCodeToControlCharacter()|.
    CheckDomCodeToMeaning("n_cc_n", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_NONE, kUndefined);
    CheckDomCodeToMeaning("n_cc_c", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_CONTROL_DOWN, kUndefined);
    CheckDomCodeToMeaning("n_cc_cs", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, kUndefined);
    // Verify |DomCodeToUsLayoutDomKey()|.
    CheckDomCodeToMeaning("n_us_n", ui::DomCodeToUsLayoutDomKey, it.dom_code,
                          ui::EF_NONE, it.normal);
    CheckDomCodeToMeaning("n_us_c", ui::DomCodeToUsLayoutDomKey, it.dom_code,
                          ui::EF_CONTROL_DOWN, it.normal);
    CheckDomCodeToMeaning("n_us_c", ui::DomCodeToUsLayoutDomKey, it.dom_code,
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, it.shift);
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
