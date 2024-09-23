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
       {true, ui::DomKey::FromCharacter(0x01), ui::VKEY_A},
       {true, ui::DomKey::FromCharacter('a'), ui::VKEY_A}},
      {ui::DomCode::US_B,
       {true, ui::DomKey::FromCharacter(0x02), ui::VKEY_B},
       {true, ui::DomKey::FromCharacter('b'), ui::VKEY_B}},
      {ui::DomCode::US_C,
       {true, ui::DomKey::FromCharacter(0x03), ui::VKEY_C},
       {true, ui::DomKey::FromCharacter('c'), ui::VKEY_C}},
      {ui::DomCode::US_D,
       {true, ui::DomKey::FromCharacter(0x04), ui::VKEY_D},
       {true, ui::DomKey::FromCharacter('d'), ui::VKEY_D}},
      {ui::DomCode::US_E,
       {true, ui::DomKey::FromCharacter(0x05), ui::VKEY_E},
       {true, ui::DomKey::FromCharacter('e'), ui::VKEY_E}},
      {ui::DomCode::US_F,
       {true, ui::DomKey::FromCharacter(0x06), ui::VKEY_F},
       {true, ui::DomKey::FromCharacter('f'), ui::VKEY_F}},
      {ui::DomCode::US_G,
       {true, ui::DomKey::FromCharacter(0x07), ui::VKEY_G},
       {true, ui::DomKey::FromCharacter('g'), ui::VKEY_G}},
      {ui::DomCode::US_H,
       {true, ui::DomKey::BACKSPACE, ui::VKEY_BACK},
       {true, ui::DomKey::FromCharacter('h'), ui::VKEY_H}},
      {ui::DomCode::US_I,
       {true, ui::DomKey::TAB, ui::VKEY_TAB},
       {true, ui::DomKey::FromCharacter('i'), ui::VKEY_I}},
      {ui::DomCode::US_J,
       {true, ui::DomKey::FromCharacter(0x0A), ui::VKEY_J},
       {true, ui::DomKey::FromCharacter('j'), ui::VKEY_J}},
      {ui::DomCode::US_K,
       {true, ui::DomKey::FromCharacter(0x0B), ui::VKEY_K},
       {true, ui::DomKey::FromCharacter('k'), ui::VKEY_K}},
      {ui::DomCode::US_L,
       {true, ui::DomKey::FromCharacter(0x0C), ui::VKEY_L},
       {true, ui::DomKey::FromCharacter('l'), ui::VKEY_L}},
      {ui::DomCode::US_M,
       {true, ui::DomKey::ENTER, ui::VKEY_RETURN},
       {true, ui::DomKey::FromCharacter('m'), ui::VKEY_M}},
      {ui::DomCode::US_N,
       {true, ui::DomKey::FromCharacter(0x0E), ui::VKEY_N},
       {true, ui::DomKey::FromCharacter('n'), ui::VKEY_N}},
      {ui::DomCode::US_O,
       {true, ui::DomKey::FromCharacter(0x0F), ui::VKEY_O},
       {true, ui::DomKey::FromCharacter('o'), ui::VKEY_O}},
      {ui::DomCode::US_P,
       {true, ui::DomKey::FromCharacter(0x10), ui::VKEY_P},
       {true, ui::DomKey::FromCharacter('p'), ui::VKEY_P}},
      {ui::DomCode::US_Q,
       {true, ui::DomKey::FromCharacter(0x11), ui::VKEY_Q},
       {true, ui::DomKey::FromCharacter('q'), ui::VKEY_Q}},
      {ui::DomCode::US_R,
       {true, ui::DomKey::FromCharacter(0x12), ui::VKEY_R},
       {true, ui::DomKey::FromCharacter('r'), ui::VKEY_R}},
      {ui::DomCode::US_S,
       {true, ui::DomKey::FromCharacter(0x13), ui::VKEY_S},
       {true, ui::DomKey::FromCharacter('s'), ui::VKEY_S}},
      {ui::DomCode::US_T,
       {true, ui::DomKey::FromCharacter(0x14), ui::VKEY_T},
       {true, ui::DomKey::FromCharacter('t'), ui::VKEY_T}},
      {ui::DomCode::US_U,
       {true, ui::DomKey::FromCharacter(0x15), ui::VKEY_U},
       {true, ui::DomKey::FromCharacter('u'), ui::VKEY_U}},
      {ui::DomCode::US_V,
       {true, ui::DomKey::FromCharacter(0x16), ui::VKEY_V},
       {true, ui::DomKey::FromCharacter('v'), ui::VKEY_V}},
      {ui::DomCode::US_W,
       {true, ui::DomKey::FromCharacter(0x17), ui::VKEY_W},
       {true, ui::DomKey::FromCharacter('w'), ui::VKEY_W}},
      {ui::DomCode::US_X,
       {true, ui::DomKey::FromCharacter(0x18), ui::VKEY_X},
       {true, ui::DomKey::FromCharacter('x'), ui::VKEY_X}},
      {ui::DomCode::US_Y,
       {true, ui::DomKey::FromCharacter(0x19), ui::VKEY_Y},
       {true, ui::DomKey::FromCharacter('y'), ui::VKEY_Y}},
      {ui::DomCode::US_Z,
       {true, ui::DomKey::FromCharacter(0x1A), ui::VKEY_Z},
       {true, ui::DomKey::FromCharacter('z'), ui::VKEY_Z}},
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
       {true, ui::DomKey::FromCharacter(0), ui::VKEY_2},
       {true, ui::DomKey::FromCharacter('2'), ui::VKEY_2},
       {true, ui::DomKey::FromCharacter('@'), ui::VKEY_2}},
      {ui::DomCode::DIGIT6,
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::FromCharacter(0x1E), ui::VKEY_6},
       {true, ui::DomKey::FromCharacter('6'), ui::VKEY_6},
       {true, ui::DomKey::FromCharacter('^'), ui::VKEY_6}},
      {ui::DomCode::MINUS,
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::FromCharacter(0x1F), ui::VKEY_OEM_MINUS},
       {true, ui::DomKey::FromCharacter('-'), ui::VKEY_OEM_MINUS},
       {true, ui::DomKey::FromCharacter('_'), ui::VKEY_OEM_MINUS}},
      {ui::DomCode::ENTER,
       {true, ui::DomKey::FromCharacter(0x0A), ui::VKEY_RETURN},
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::ENTER, ui::VKEY_RETURN},
       {true, ui::DomKey::ENTER, ui::VKEY_RETURN}},
      {ui::DomCode::BRACKET_LEFT,
       {true, ui::DomKey::ESCAPE, ui::VKEY_OEM_4},
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::FromCharacter('['), ui::VKEY_OEM_4},
       {true, ui::DomKey::FromCharacter('{'), ui::VKEY_OEM_4}},
      {ui::DomCode::BACKSLASH,
       {true, ui::DomKey::FromCharacter(0x1C), ui::VKEY_OEM_5},
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::FromCharacter('\\'), ui::VKEY_OEM_5},
       {true, ui::DomKey::FromCharacter('|'), ui::VKEY_OEM_5}},
      {ui::DomCode::BRACKET_RIGHT,
       {true, ui::DomKey::FromCharacter(0x1D), ui::VKEY_OEM_6},
       {false, ui::DomKey::NONE, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::FromCharacter(']'), ui::VKEY_OEM_6},
       {true, ui::DomKey::FromCharacter('}'), ui::VKEY_OEM_6}},
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
       {true, ui::DomKey::FromCharacter('1'), ui::VKEY_1},
       {true, ui::DomKey::FromCharacter('!'), ui::VKEY_1}},
      {ui::DomCode::EQUAL,
       {true, ui::DomKey::FromCharacter('='), ui::VKEY_OEM_PLUS},
       {true, ui::DomKey::FromCharacter('+'), ui::VKEY_OEM_PLUS}},
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
       {true, ui::DomKey::FromCharacter('a'), ui::VKEY_A},
       {true, ui::DomKey::FromCharacter('A'), ui::VKEY_A}},
      {ui::DomCode::US_B,
       {true, ui::DomKey::FromCharacter('b'), ui::VKEY_B},
       {true, ui::DomKey::FromCharacter('B'), ui::VKEY_B}},
      {ui::DomCode::US_C,
       {true, ui::DomKey::FromCharacter('c'), ui::VKEY_C},
       {true, ui::DomKey::FromCharacter('C'), ui::VKEY_C}},
      {ui::DomCode::US_D,
       {true, ui::DomKey::FromCharacter('d'), ui::VKEY_D},
       {true, ui::DomKey::FromCharacter('D'), ui::VKEY_D}},
      {ui::DomCode::US_E,
       {true, ui::DomKey::FromCharacter('e'), ui::VKEY_E},
       {true, ui::DomKey::FromCharacter('E'), ui::VKEY_E}},
      {ui::DomCode::US_F,
       {true, ui::DomKey::FromCharacter('f'), ui::VKEY_F},
       {true, ui::DomKey::FromCharacter('F'), ui::VKEY_F}},
      {ui::DomCode::US_G,
       {true, ui::DomKey::FromCharacter('g'), ui::VKEY_G},
       {true, ui::DomKey::FromCharacter('G'), ui::VKEY_G}},
      {ui::DomCode::US_H,
       {true, ui::DomKey::FromCharacter('h'), ui::VKEY_H},
       {true, ui::DomKey::FromCharacter('H'), ui::VKEY_H}},
      {ui::DomCode::US_I,
       {true, ui::DomKey::FromCharacter('i'), ui::VKEY_I},
       {true, ui::DomKey::FromCharacter('I'), ui::VKEY_I}},
      {ui::DomCode::US_J,
       {true, ui::DomKey::FromCharacter('j'), ui::VKEY_J},
       {true, ui::DomKey::FromCharacter('J'), ui::VKEY_J}},
      {ui::DomCode::US_K,
       {true, ui::DomKey::FromCharacter('k'), ui::VKEY_K},
       {true, ui::DomKey::FromCharacter('K'), ui::VKEY_K}},
      {ui::DomCode::US_L,
       {true, ui::DomKey::FromCharacter('l'), ui::VKEY_L},
       {true, ui::DomKey::FromCharacter('L'), ui::VKEY_L}},
      {ui::DomCode::US_M,
       {true, ui::DomKey::FromCharacter('m'), ui::VKEY_M},
       {true, ui::DomKey::FromCharacter('M'), ui::VKEY_M}},
      {ui::DomCode::US_N,
       {true, ui::DomKey::FromCharacter('n'), ui::VKEY_N},
       {true, ui::DomKey::FromCharacter('N'), ui::VKEY_N}},
      {ui::DomCode::US_O,
       {true, ui::DomKey::FromCharacter('o'), ui::VKEY_O},
       {true, ui::DomKey::FromCharacter('O'), ui::VKEY_O}},
      {ui::DomCode::US_P,
       {true, ui::DomKey::FromCharacter('p'), ui::VKEY_P},
       {true, ui::DomKey::FromCharacter('P'), ui::VKEY_P}},
      {ui::DomCode::US_Q,
       {true, ui::DomKey::FromCharacter('q'), ui::VKEY_Q},
       {true, ui::DomKey::FromCharacter('Q'), ui::VKEY_Q}},
      {ui::DomCode::US_R,
       {true, ui::DomKey::FromCharacter('r'), ui::VKEY_R},
       {true, ui::DomKey::FromCharacter('R'), ui::VKEY_R}},
      {ui::DomCode::US_S,
       {true, ui::DomKey::FromCharacter('s'), ui::VKEY_S},
       {true, ui::DomKey::FromCharacter('S'), ui::VKEY_S}},
      {ui::DomCode::US_T,
       {true, ui::DomKey::FromCharacter('t'), ui::VKEY_T},
       {true, ui::DomKey::FromCharacter('T'), ui::VKEY_T}},
      {ui::DomCode::US_U,
       {true, ui::DomKey::FromCharacter('u'), ui::VKEY_U},
       {true, ui::DomKey::FromCharacter('U'), ui::VKEY_U}},
      {ui::DomCode::US_V,
       {true, ui::DomKey::FromCharacter('v'), ui::VKEY_V},
       {true, ui::DomKey::FromCharacter('V'), ui::VKEY_V}},
      {ui::DomCode::US_W,
       {true, ui::DomKey::FromCharacter('w'), ui::VKEY_W},
       {true, ui::DomKey::FromCharacter('W'), ui::VKEY_W}},
      {ui::DomCode::US_X,
       {true, ui::DomKey::FromCharacter('x'), ui::VKEY_X},
       {true, ui::DomKey::FromCharacter('X'), ui::VKEY_X}},
      {ui::DomCode::US_Y,
       {true, ui::DomKey::FromCharacter('y'), ui::VKEY_Y},
       {true, ui::DomKey::FromCharacter('Y'), ui::VKEY_Y}},
      {ui::DomCode::US_Z,
       {true, ui::DomKey::FromCharacter('z'), ui::VKEY_Z},
       {true, ui::DomKey::FromCharacter('Z'), ui::VKEY_Z}},
      {ui::DomCode::DIGIT1,
       {true, ui::DomKey::FromCharacter('1'), ui::VKEY_1},
       {true, ui::DomKey::FromCharacter('!'), ui::VKEY_1}},
      {ui::DomCode::DIGIT2,
       {true, ui::DomKey::FromCharacter('2'), ui::VKEY_2},
       {true, ui::DomKey::FromCharacter('@'), ui::VKEY_2}},
      {ui::DomCode::DIGIT3,
       {true, ui::DomKey::FromCharacter('3'), ui::VKEY_3},
       {true, ui::DomKey::FromCharacter('#'), ui::VKEY_3}},
      {ui::DomCode::DIGIT4,
       {true, ui::DomKey::FromCharacter('4'), ui::VKEY_4},
       {true, ui::DomKey::FromCharacter('$'), ui::VKEY_4}},
      {ui::DomCode::DIGIT5,
       {true, ui::DomKey::FromCharacter('5'), ui::VKEY_5},
       {true, ui::DomKey::FromCharacter('%'), ui::VKEY_5}},
      {ui::DomCode::DIGIT6,
       {true, ui::DomKey::FromCharacter('6'), ui::VKEY_6},
       {true, ui::DomKey::FromCharacter('^'), ui::VKEY_6}},
      {ui::DomCode::DIGIT7,
       {true, ui::DomKey::FromCharacter('7'), ui::VKEY_7},
       {true, ui::DomKey::FromCharacter('&'), ui::VKEY_7}},
      {ui::DomCode::DIGIT8,
       {true, ui::DomKey::FromCharacter('8'), ui::VKEY_8},
       {true, ui::DomKey::FromCharacter('*'), ui::VKEY_8}},
      {ui::DomCode::DIGIT9,
       {true, ui::DomKey::FromCharacter('9'), ui::VKEY_9},
       {true, ui::DomKey::FromCharacter('('), ui::VKEY_9}},
      {ui::DomCode::DIGIT0,
       {true, ui::DomKey::FromCharacter('0'), ui::VKEY_0},
       {true, ui::DomKey::FromCharacter(')'), ui::VKEY_0}},
      {ui::DomCode::SPACE,
       {true, ui::DomKey::FromCharacter(' '), ui::VKEY_SPACE},
       {true, ui::DomKey::FromCharacter(' '), ui::VKEY_SPACE}},
      {ui::DomCode::MINUS,
       {true, ui::DomKey::FromCharacter('-'), ui::VKEY_OEM_MINUS},
       {true, ui::DomKey::FromCharacter('_'), ui::VKEY_OEM_MINUS}},
      {ui::DomCode::EQUAL,
       {true, ui::DomKey::FromCharacter('='), ui::VKEY_OEM_PLUS},
       {true, ui::DomKey::FromCharacter('+'), ui::VKEY_OEM_PLUS}},
      {ui::DomCode::BRACKET_LEFT,
       {true, ui::DomKey::FromCharacter('['), ui::VKEY_OEM_4},
       {true, ui::DomKey::FromCharacter('{'), ui::VKEY_OEM_4}},
      {ui::DomCode::BRACKET_RIGHT,
       {true, ui::DomKey::FromCharacter(']'), ui::VKEY_OEM_6},
       {true, ui::DomKey::FromCharacter('}'), ui::VKEY_OEM_6}},
      {ui::DomCode::BACKSLASH,
       {true, ui::DomKey::FromCharacter('\\'), ui::VKEY_OEM_5},
       {true, ui::DomKey::FromCharacter('|'), ui::VKEY_OEM_5}},
      {ui::DomCode::SEMICOLON,
       {true, ui::DomKey::FromCharacter(';'), ui::VKEY_OEM_1},
       {true, ui::DomKey::FromCharacter(':'), ui::VKEY_OEM_1}},
      {ui::DomCode::QUOTE,
       {true, ui::DomKey::FromCharacter('\''), ui::VKEY_OEM_7},
       {true, ui::DomKey::FromCharacter('"'), ui::VKEY_OEM_7}},
      {ui::DomCode::BACKQUOTE,
       {true, ui::DomKey::FromCharacter('`'), ui::VKEY_OEM_3},
       {true, ui::DomKey::FromCharacter('~'), ui::VKEY_OEM_3}},
      {ui::DomCode::COMMA,
       {true, ui::DomKey::FromCharacter(','), ui::VKEY_OEM_COMMA},
       {true, ui::DomKey::FromCharacter('<'), ui::VKEY_OEM_COMMA}},
      {ui::DomCode::PERIOD,
       {true, ui::DomKey::FromCharacter('.'), ui::VKEY_OEM_PERIOD},
       {true, ui::DomKey::FromCharacter('>'), ui::VKEY_OEM_PERIOD}},
      {ui::DomCode::SLASH,
       {true, ui::DomKey::FromCharacter('/'), ui::VKEY_OEM_2},
       {true, ui::DomKey::FromCharacter('?'), ui::VKEY_OEM_2}},
      {ui::DomCode::INTL_BACKSLASH,
       {true, ui::DomKey::FromCharacter('<'), ui::VKEY_OEM_102},
       {true, ui::DomKey::FromCharacter('>'), ui::VKEY_OEM_102}},
      {ui::DomCode::INTL_YEN,
       {true, ui::DomKey::FromCharacter(0x00A5), ui::VKEY_OEM_5},
       {true, ui::DomKey::FromCharacter('|'), ui::VKEY_OEM_5}},
      {ui::DomCode::NUMPAD_DIVIDE,
       {true, ui::DomKey::FromCharacter('/'), ui::VKEY_DIVIDE},
       {true, ui::DomKey::FromCharacter('/'), ui::VKEY_DIVIDE}},
      {ui::DomCode::NUMPAD_MULTIPLY,
       {true, ui::DomKey::FromCharacter('*'), ui::VKEY_MULTIPLY},
       {true, ui::DomKey::FromCharacter('*'), ui::VKEY_MULTIPLY}},
      {ui::DomCode::NUMPAD_SUBTRACT,
       {true, ui::DomKey::FromCharacter('-'), ui::VKEY_SUBTRACT},
       {true, ui::DomKey::FromCharacter('-'), ui::VKEY_SUBTRACT}},
      {ui::DomCode::NUMPAD_ADD,
       {true, ui::DomKey::FromCharacter('+'), ui::VKEY_ADD},
       {true, ui::DomKey::FromCharacter('+'), ui::VKEY_ADD}},
      {ui::DomCode::NUMPAD1,
       {true, ui::DomKey::FromCharacter('1'), ui::VKEY_1},
       {true, ui::DomKey::FromCharacter('1'), ui::VKEY_1}},
      {ui::DomCode::NUMPAD2,
       {true, ui::DomKey::FromCharacter('2'), ui::VKEY_2},
       {true, ui::DomKey::FromCharacter('2'), ui::VKEY_2}},
      {ui::DomCode::NUMPAD3,
       {true, ui::DomKey::FromCharacter('3'), ui::VKEY_3},
       {true, ui::DomKey::FromCharacter('3'), ui::VKEY_3}},
      {ui::DomCode::NUMPAD4,
       {true, ui::DomKey::FromCharacter('4'), ui::VKEY_4},
       {true, ui::DomKey::FromCharacter('4'), ui::VKEY_4}},
      {ui::DomCode::NUMPAD5,
       {true, ui::DomKey::FromCharacter('5'), ui::VKEY_5},
       {true, ui::DomKey::FromCharacter('5'), ui::VKEY_5}},
      {ui::DomCode::NUMPAD6,
       {true, ui::DomKey::FromCharacter('6'), ui::VKEY_6},
       {true, ui::DomKey::FromCharacter('6'), ui::VKEY_6}},
      {ui::DomCode::NUMPAD7,
       {true, ui::DomKey::FromCharacter('7'), ui::VKEY_7},
       {true, ui::DomKey::FromCharacter('7'), ui::VKEY_7}},
      {ui::DomCode::NUMPAD8,
       {true, ui::DomKey::FromCharacter('8'), ui::VKEY_8},
       {true, ui::DomKey::FromCharacter('8'), ui::VKEY_8}},
      {ui::DomCode::NUMPAD9,
       {true, ui::DomKey::FromCharacter('9'), ui::VKEY_9},
       {true, ui::DomKey::FromCharacter('9'), ui::VKEY_9}},
      {ui::DomCode::NUMPAD0,
       {true, ui::DomKey::FromCharacter('0'), ui::VKEY_0},
       {true, ui::DomKey::FromCharacter('0'), ui::VKEY_0}},
      {ui::DomCode::NUMPAD_DECIMAL,
       {true, ui::DomKey::FromCharacter('.'), ui::VKEY_DECIMAL},
       {true, ui::DomKey::FromCharacter('.'), ui::VKEY_DECIMAL}},
      {ui::DomCode::NUMPAD_EQUAL,
       {true, ui::DomKey::FromCharacter('='), ui::VKEY_UNKNOWN},
       {true, ui::DomKey::FromCharacter('='), ui::VKEY_UNKNOWN}},
      {ui::DomCode::NUMPAD_COMMA,
       {true, ui::DomKey::FromCharacter(','), ui::VKEY_OEM_COMMA},
       {true, ui::DomKey::FromCharacter(','), ui::VKEY_OEM_COMMA}},
      {ui::DomCode::NUMPAD_PAREN_LEFT,
       {true, ui::DomKey::FromCharacter('('), ui::VKEY_UNKNOWN},
       {true, ui::DomKey::FromCharacter('('), ui::VKEY_UNKNOWN}},
      {ui::DomCode::NUMPAD_PAREN_RIGHT,
       {true, ui::DomKey::FromCharacter(')'), ui::VKEY_UNKNOWN},
       {true, ui::DomKey::FromCharacter(')'), ui::VKEY_UNKNOWN}},
      {ui::DomCode::NUMPAD_SIGN_CHANGE,
       {true, ui::DomKey::FromCharacter(0xB1), ui::VKEY_UNKNOWN},
       {true, ui::DomKey::FromCharacter(0xB1), ui::VKEY_UNKNOWN}},
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
