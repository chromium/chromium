// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"

#include <stddef.h>
#include <xkbcommon/xkbcommon-names.h>

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_xkb.h"

namespace ui {

namespace {

typedef base::OnceCallback<void(const std::string&,
                                std::unique_ptr<char, base::FreeDeleter>)>
    LoadKeymapCallback;

KeyboardCode AlphanumericKeyboardCode(xkb_keysym_t xkb_keysym,
                                      char16_t character) {
  // Plain ASCII letters and digits map directly to VKEY values.
  if ((character >= '0') && (character <= '9')) {
    int zero = ((xkb_keysym >= XKB_KEY_KP_0) && (xkb_keysym <= XKB_KEY_KP_9))
                   ? VKEY_NUMPAD0
                   : VKEY_0;
    return static_cast<KeyboardCode>(zero + character - '0');
  }
  if ((character >= 'a') && (character <= 'z'))
    return static_cast<KeyboardCode>(VKEY_A + character - 'a');
  if ((character >= 'A') && (character <= 'Z'))
    return static_cast<KeyboardCode>(VKEY_A + character - 'A');
  return VKEY_UNKNOWN;
}

// These tables map layout-dependent printable characters/codes
// to legacy Windows-based VKEY values.
//
// VKEYs are determined by the character produced from a DomCode without
// any modifiers, plus zero or more of the DomCode itself, the character
// produced with the Shift modifier, and the character produced with the
// AltGr modifier.

// A table of one or more PrintableSubEntry cases applies when the VKEY is
// not determined by the unmodified character value alone. Each such table
// corresponds to one unmodified character value. For an entry to match,
// the dom_code must match, and, if test_X is set, then the character for
// the key plus modifier X must also match.
struct PrintableSubEntry {
  DomCode dom_code;
  bool test_shift : 1;
  bool test_altgr : 1;
  char16_t shift_character;
  char16_t altgr_character;
  KeyboardCode key_code;
};

// The two designated Unicode "not-a-character" values are used as sentinels.
const char16_t kNone = 0xFFFE;
const char16_t kAny = 0xFFFF;

// U+0021 exclamation mark
const PrintableSubEntry kU0021[] = {
    {DomCode::DIGIT1, false, false, kAny, kAny, VKEY_1},
    {DomCode::DIGIT8, false, false, kAny, kAny, VKEY_8},
    {DomCode::SLASH, false, false, kAny, kAny, VKEY_OEM_8}};

// U+0022 quote
const PrintableSubEntry kU0022[] = {
    {DomCode::DIGIT2, false, false, kAny, kAny, VKEY_2},
    {DomCode::DIGIT3, false, false, kAny, kAny, VKEY_3}};

// U+0023 number sign
const PrintableSubEntry kU0023[] = {
    {DomCode::BACKQUOTE, false, false, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKSLASH, true, false, 0x0027, kAny, VKEY_OEM_2},  // apostrophe
    {DomCode::BACKSLASH, true, false, 0x007E, kAny,
     VKEY_OEM_7}};  // ~, NoSymbol

// U+0024 dollar sign
const PrintableSubEntry kU0024[] = {
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_1},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_8}};

// U+0027 apostrophe
const PrintableSubEntry kU0027[] = {
    {DomCode::DIGIT4, false, false, kAny, kAny, VKEY_4},
    {DomCode::US_Q, false, false, kAny, kAny, VKEY_OEM_7},
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_1},
    {DomCode::SLASH, false, false, kAny, kAny, VKEY_OEM_7},
    {DomCode::QUOTE, true, false, 0x0022, kAny, VKEY_OEM_7},      // quote
    {DomCode::BACKQUOTE, true, false, 0x0022, kAny, VKEY_OEM_3},  // quote
    {DomCode::BACKQUOTE, true, false, 0x00B7, kAny, VKEY_OEM_5},  // middle dot
    {DomCode::BACKSLASH, true, false, kNone, kAny, VKEY_OEM_5},   // NoSymbol
    {DomCode::MINUS, true, false, 0x003F, kAny, VKEY_OEM_4},      // ?
    {DomCode::EQUAL, true, false, 0x002A, kAny, VKEY_OEM_PLUS},   // *
    {DomCode::QUOTE, true, false, 0x0040, kAny, VKEY_OEM_3},      // @
    {DomCode::BACKSLASH, true, true, 0x002A, 0x00BD,
     VKEY_OEM_5},                                                 // *, one half
    {DomCode::BACKSLASH, true, false, 0x002A, kAny, VKEY_OEM_2},  // *, NoSymbol
    {DomCode::US_Z, true, true, 0x0022, 0x0158, VKEY_OEM_7},  // quote, R caron
    {DomCode::US_Z, true, false, 0x0022, kAny, VKEY_Z}};      // quote

// U+0028 left parenthesis
const PrintableSubEntry kU0028[] = {
    {DomCode::DIGIT5, false, false, kAny, kAny, VKEY_5},
    {DomCode::DIGIT9, false, false, kAny, kAny, VKEY_9},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_5}};

// U+0029 right parenthesis
const PrintableSubEntry kU0029[] = {
    {DomCode::DIGIT0, false, false, kAny, kAny, VKEY_0},
    {DomCode::MINUS, false, false, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_6}};

// U+002A *
const PrintableSubEntry kU002A[] = {
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_1},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_5}};

// U+002B plus sign
const PrintableSubEntry kU002B[] = {
    {DomCode::DIGIT1, false, false, kAny, kAny, VKEY_1},
    {DomCode::EQUAL, false, false, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::SEMICOLON, false, false, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_2},
    {DomCode::MINUS, true, false, 0x003F, kAny, VKEY_OEM_PLUS}};  // ?

// U+002C comma
const PrintableSubEntry kU002C[] = {
    {DomCode::DIGIT3, false, false, kAny, kAny, VKEY_3},
    {DomCode::DIGIT5, false, false, kAny, kAny, VKEY_5},
    {DomCode::DIGIT6, false, false, kAny, kAny, VKEY_6},
    {DomCode::DIGIT9, false, false, kAny, kAny, VKEY_9},
    {DomCode::US_W, false, false, kAny, kAny, VKEY_OEM_COMMA},
    {DomCode::US_V, false, false, kAny, kAny, VKEY_OEM_COMMA},
    {DomCode::US_M, false, false, kAny, kAny, VKEY_OEM_COMMA},
    {DomCode::COMMA, false, false, kAny, kAny, VKEY_OEM_COMMA}};

// U+002D hyphen-minus
const PrintableSubEntry kU002D[] = {
    {DomCode::DIGIT2, false, false, kAny, kAny, VKEY_2},
    {DomCode::DIGIT6, false, false, kAny, kAny, VKEY_6},
    {DomCode::MINUS, false, false, kAny, kAny, VKEY_OEM_MINUS},
    {DomCode::US_A, false, false, kAny, kAny, VKEY_OEM_MINUS},
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_MINUS},
    {DomCode::SLASH, true, false, 0x003D, kAny, VKEY_OEM_MINUS},  // =
    {DomCode::EQUAL, true, true, 0x005F, 0x0157, VKEY_OEM_4},  // _, r cedilla
    {DomCode::EQUAL, true, false, 0x005F, kAny, VKEY_OEM_MINUS},   // _
    {DomCode::SLASH, true, true, 0x005F, 0x002F, VKEY_OEM_2},      // _, /
    {DomCode::SLASH, true, false, 0x005F, kAny, VKEY_OEM_MINUS}};  // _

// U+002E full stop
const PrintableSubEntry kU002E[] = {
    {DomCode::DIGIT7, false, false, kAny, kAny, VKEY_7},
    {DomCode::DIGIT8, false, false, kAny, kAny, VKEY_8},
    {DomCode::US_E, false, false, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::US_R, false, false, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::US_O, false, false, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_7},
    {DomCode::PERIOD, false, false, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::SLASH, false, false, kAny, kAny, VKEY_OEM_2}};

// U+002F /
const PrintableSubEntry kU002F[] = {
    {DomCode::MINUS, false, false, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_OEM_2},
    {DomCode::BACKQUOTE, false, false, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_5},
    {DomCode::SLASH, false, false, kAny, kAny, VKEY_OEM_2},
    {DomCode::CONTROL_RIGHT, false, false, kAny, kAny, VKEY_RCONTROL},
    {DomCode::DIGIT3, true, false, 0x0033, kAny, VKEY_3},       // 3
    {DomCode::DIGIT3, true, false, 0x003F, kAny, VKEY_OEM_2},   // ?
    {DomCode::DIGIT0, true, false, 0x0030, kAny, VKEY_0},       // 0
    {DomCode::DIGIT0, true, false, 0x003F, kAny, VKEY_OEM_2}};  // ?

// U+003A colon
const PrintableSubEntry kU003A[] = {
    {DomCode::DIGIT1, false, false, kAny, kAny, VKEY_1},
    {DomCode::DIGIT5, false, false, kAny, kAny, VKEY_5},
    {DomCode::DIGIT6, false, false, kAny, kAny, VKEY_6},
    {DomCode::PERIOD, false, false, kAny, kAny, VKEY_OEM_2}};

// U+003B semicolon
const PrintableSubEntry kU003B[] = {
    {DomCode::DIGIT4, false, false, kAny, kAny, VKEY_4},
    {DomCode::DIGIT8, false, false, kAny, kAny, VKEY_8},
    {DomCode::US_Q, false, false, kAny, kAny, VKEY_OEM_1},
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_OEM_1},
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_6},
    {DomCode::SEMICOLON, false, false, kAny, kAny, VKEY_OEM_1},
    {DomCode::BACKQUOTE, false, false, kAny, kAny, VKEY_OEM_3},
    {DomCode::US_Z, false, false, kAny, kAny, VKEY_OEM_1},
    {DomCode::COMMA, false, false, kAny, kAny, VKEY_OEM_PERIOD},
    {DomCode::SLASH, false, false, kAny, kAny, VKEY_OEM_2}};
// U+003D =
const PrintableSubEntry kU003D[] = {
    {DomCode::DIGIT8, false, false, kAny, kAny, VKEY_8},
    {DomCode::EQUAL, false, false, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::SLASH, true, false, 0x0025, kAny, VKEY_OEM_8},       // %
    {DomCode::SLASH, true, false, 0x002B, kAny, VKEY_OEM_PLUS},    // +
    {DomCode::MINUS, true, true, 0x0025, 0x002D, VKEY_OEM_MINUS},  // %, -
    {DomCode::MINUS, true, false, 0x0025, kAny, VKEY_OEM_PLUS}};  // %, NoSymbol

// U+003F ?
const PrintableSubEntry kU003F[] = {
    {DomCode::DIGIT2, false, false, kAny, kAny, VKEY_2},
    {DomCode::DIGIT7, false, false, kAny, kAny, VKEY_7},
    {DomCode::DIGIT8, false, false, kAny, kAny, VKEY_8},
    {DomCode::MINUS, false, false, kAny, kAny, VKEY_OEM_PLUS}};

// U+0040 @
const PrintableSubEntry kU0040[] = {
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_6},
    {DomCode::BACKQUOTE, false, false, kAny, kAny, VKEY_OEM_7}};

// U+005B left square bracket
const PrintableSubEntry kU005B[] = {
    {DomCode::DIGIT1, false, false, kAny, kAny, VKEY_OEM_4},
    {DomCode::MINUS, false, false, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_6},
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_7}};

// U+005C backslash
const PrintableSubEntry kU005C[] = {
    {DomCode::EQUAL, false, false, kAny, kAny, VKEY_OEM_4},
    {DomCode::BACKSLASH, true, false, 0x002F, kAny, VKEY_OEM_7},   // /
    {DomCode::BACKSLASH, true, false, 0x007C, kAny, VKEY_OEM_5},   // |
    {DomCode::BACKQUOTE, true, true, 0x007C, 0x0031, VKEY_OEM_5},  // |, 1
    {DomCode::BACKQUOTE, true, true, 0x007C, 0x0145,
     VKEY_OEM_3}};  // |, N cedilla

// U+005D right square bracket
const PrintableSubEntry kU005D[] = {
    {DomCode::DIGIT2, false, false, kAny, kAny, VKEY_OEM_6},
    {DomCode::EQUAL, false, false, kAny, kAny, VKEY_OEM_6},
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_6},
    {DomCode::BACKQUOTE, false, false, kAny, kAny, VKEY_OEM_3},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_5}};

// U+005F _
const PrintableSubEntry kU005F[] = {
    {DomCode::DIGIT8, false, false, kAny, kAny, VKEY_8},
    {DomCode::MINUS, false, false, kAny, kAny, VKEY_OEM_MINUS}};

// U+0060 grave accent
const PrintableSubEntry kU0060[] = {
    {DomCode::BACKQUOTE, true, false, kNone, kAny, VKEY_OEM_3},    // NoSymbol
    {DomCode::BACKQUOTE, true, false, 0x00AC, kAny, VKEY_OEM_8},   // not
    {DomCode::BACKQUOTE, true, false, 0x007E, kAny, VKEY_OEM_3}};  // ~

// U+00A7 section
const PrintableSubEntry kU00A7[] = {
    {DomCode::DIGIT4, false, false, kAny, kAny, VKEY_4},
    {DomCode::DIGIT6, false, false, kAny, kAny, VKEY_6},
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKQUOTE, true, false, 0x00B0, kAny, VKEY_OEM_2},   // degree
    {DomCode::BACKQUOTE, true, false, 0x00BD, kAny, VKEY_OEM_5}};  // one half

// U+00AB left-pointing double angle quote
const PrintableSubEntry kU00AB[] = {
    {DomCode::DIGIT8, false, false, kAny, kAny, VKEY_8},
    {DomCode::EQUAL, false, false, kAny, kAny, VKEY_OEM_6}};

// U+00B0 degree
const PrintableSubEntry kU00B0[] = {
    {DomCode::EQUAL, false, false, kAny, kAny, VKEY_OEM_2},
    {DomCode::BACKQUOTE, false, false, kAny, kAny, VKEY_OEM_7}};

// U+00BA masculine ordinal indicator
const PrintableSubEntry kU00BA[] = {
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKQUOTE, false, false, kAny, kAny, VKEY_OEM_5}};

// U+00E0 a grave
const PrintableSubEntry kU00E0[] = {
    {DomCode::DIGIT0, false, false, kAny, kAny, VKEY_0},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_5},
    {DomCode::QUOTE, true, false, 0x00B0, kAny, VKEY_OEM_7},   // degree
    {DomCode::QUOTE, true, false, 0x00E4, kAny, VKEY_OEM_5}};  // a diaeresis

// U+00E1 a acute
const PrintableSubEntry kU00E1[] = {
    {DomCode::DIGIT8, false, false, kAny, kAny, VKEY_8},
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_7}};

// U+00E2 a circumflex
const PrintableSubEntry kU00E2[] = {
    {DomCode::DIGIT2, false, false, kAny, kAny, VKEY_2},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_5}};

// U+00E4 a diaeresis
const PrintableSubEntry kU00E4[] = {
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_6},
    {DomCode::QUOTE, true, false, 0x00E0, kAny, VKEY_OEM_5},   // a grave
    {DomCode::QUOTE, true, false, 0x00C4, kAny, VKEY_OEM_7}};  // A dia.

// U+00E6 ae
const PrintableSubEntry kU00E6[] = {
    {DomCode::SEMICOLON, false, false, kAny, kAny, VKEY_OEM_3},
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_7}};

// U+00E7 c cedilla
const PrintableSubEntry kU00E7[] = {
    {DomCode::DIGIT9, false, false, kAny, kAny, VKEY_9},
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_6},
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_7},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_2},
    {DomCode::COMMA, false, false, kAny, kAny, VKEY_OEM_COMMA},
    {DomCode::SEMICOLON, true, true, 0x00C7, 0x00DE,
     VKEY_OEM_3},  // C ced., Thorn
    {DomCode::SEMICOLON, true, false, 0x00C7, kAny,
     VKEY_OEM_1}};  // C ced., NoSy

// U+00E8 e grave
const PrintableSubEntry kU00E8[] = {
    {DomCode::DIGIT7, false, false, kAny, kAny, VKEY_7},
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_OEM_1},
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_3}};

// U+00E9 e acute
const PrintableSubEntry kU00E9[] = {
    {DomCode::DIGIT2, false, false, kAny, kAny, VKEY_2},
    {DomCode::DIGIT0, false, false, kAny, kAny, VKEY_0},
    {DomCode::SLASH, false, false, kAny, kAny, VKEY_OEM_2},
    {DomCode::SEMICOLON, true, false, 0x00C9, kAny, VKEY_OEM_1},  // E acute
    {DomCode::SEMICOLON, true, false, 0x00F6, kAny,
     VKEY_OEM_7}};  // o diaeresis

// U+00ED i acute
const PrintableSubEntry kU00ED[] = {
    {DomCode::DIGIT9, false, false, kAny, kAny, VKEY_9},
    {DomCode::BACKQUOTE, false, false, kAny, kAny, VKEY_0}};

// U+00F0 eth
const PrintableSubEntry kU00F0[] = {
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_OEM_6},
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_1}};

// U+00F3 o acute
const PrintableSubEntry kU00F3[] = {
    {DomCode::EQUAL, false, false, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_5}};

// U+00F4 o circumflex
const PrintableSubEntry kU00F4[] = {
    {DomCode::DIGIT4, false, false, kAny, kAny, VKEY_4},
    {DomCode::SEMICOLON, false, false, kAny, kAny, VKEY_OEM_1}};

// U+00F6 o diaeresis
const PrintableSubEntry kU00F6[] = {
    {DomCode::DIGIT0, false, false, kAny, kAny, VKEY_OEM_3},
    {DomCode::MINUS, false, false, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_OEM_4},
    {DomCode::SEMICOLON, true, false, 0x00E9, kAny, VKEY_OEM_7},  // e acute
    {DomCode::SEMICOLON, true, true, 0x00D6, 0x0162,
     VKEY_OEM_3},  // O dia., T ced.
    {DomCode::SEMICOLON, true, false, 0x00D6, kAny, VKEY_OEM_3}};  // O diaresis

// U+00F8 o stroke
const PrintableSubEntry kU00F8[] = {
    {DomCode::SEMICOLON, false, false, kAny, kAny, VKEY_OEM_3},
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_7}};

// U+00F9 u grave
const PrintableSubEntry kU00F9[] = {
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_3},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_2}};

// U+00FA u acute
const PrintableSubEntry kU00FA[] = {
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_OEM_4},
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_6}};

// U+00FC u diaeresis
const PrintableSubEntry kU00FC[] = {
    {DomCode::US_W, false, false, kAny, kAny, VKEY_W},
    {DomCode::BRACKET_LEFT, true, false, 0x00E8, kAny, VKEY_OEM_1},  // e grave
    {DomCode::MINUS, true, false, 0x00DC, kAny, VKEY_OEM_2},  // U diaresis
    {DomCode::BRACKET_LEFT, true, true, 0x00DC, 0x0141,
     VKEY_OEM_3},  // U dia., L-
    {DomCode::BRACKET_LEFT, true, false, 0x00DC, kAny,
     VKEY_OEM_1}};  // U diaresis

// U+0103 a breve
const PrintableSubEntry kU0103[] = {
    {DomCode::DIGIT1, false, false, kAny, kAny, VKEY_1},
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_OEM_4}};

// U+0105 a ogonek
const PrintableSubEntry kU0105[] = {
    {DomCode::DIGIT1, false, false, kAny, kAny, VKEY_1},
    {DomCode::US_Q, false, false, kAny, kAny, VKEY_Q},
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_7}};

// U+010D c caron
const PrintableSubEntry kU010D[] = {
    {DomCode::DIGIT2, false, false, kAny, kAny, VKEY_2},
    {DomCode::DIGIT4, false, false, kAny, kAny, VKEY_4},
    {DomCode::US_P, false, false, kAny, kAny, VKEY_X},
    {DomCode::SEMICOLON, false, false, kAny, kAny, VKEY_OEM_1},
    {DomCode::COMMA, false, false, kAny, kAny, VKEY_OEM_COMMA}};

// U+0111 d stroke
const PrintableSubEntry kU0111[] = {
    {DomCode::DIGIT0, false, false, kAny, kAny, VKEY_0},
    {DomCode::BRACKET_RIGHT, false, false, kAny, kAny, VKEY_OEM_6}};

// U+0117 e dot above
const PrintableSubEntry kU0117[] = {
    {DomCode::DIGIT4, false, false, kAny, kAny, VKEY_4},
    {DomCode::QUOTE, false, false, kAny, kAny, VKEY_OEM_7}};

// U+0119 e ogonek
const PrintableSubEntry kU0119[] = {
    {DomCode::DIGIT3, false, false, kAny, kAny, VKEY_3},
    {DomCode::SLASH, true, true, 0x0118, 0x006E, VKEY_OEM_2},  // E ogonek, n
    {DomCode::SLASH, true, false, 0x0118, kAny, VKEY_OEM_MINUS}};  // E ogonek

// U+012F i ogonek
const PrintableSubEntry kU012F[] = {
    {DomCode::DIGIT5, false, false, kAny, kAny, VKEY_5},
    {DomCode::BRACKET_LEFT, true, false, 0x012E, kAny, VKEY_OEM_4}};  // Iogonek

// U+0142 l stroke
const PrintableSubEntry kU0142[] = {
    {DomCode::SEMICOLON, false, false, kAny, kAny, VKEY_OEM_1},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_2}};

// U+015F s cedilla
const PrintableSubEntry kU015F[] = {
    {DomCode::SEMICOLON, false, false, kAny, kAny, VKEY_OEM_1},
    {DomCode::PERIOD, false, false, kAny, kAny, VKEY_OEM_PERIOD}};

// U+0161 s caron
const PrintableSubEntry kU0161[] = {
    {DomCode::DIGIT3, false, false, kAny, kAny, VKEY_3},
    {DomCode::DIGIT6, false, false, kAny, kAny, VKEY_6},
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_OEM_4},
    {DomCode::US_A, false, false, kAny, kAny, VKEY_OEM_1},
    {DomCode::US_F, false, false, kAny, kAny, VKEY_F},
    {DomCode::PERIOD, false, false, kAny, kAny, VKEY_OEM_PERIOD}};

// U+016B u macron
const PrintableSubEntry kU016B[] = {
    {DomCode::DIGIT8, false, false, kAny, kAny, VKEY_8},
    {DomCode::US_Q, false, false, kAny, kAny, VKEY_Q},
    {DomCode::US_X, false, false, kAny, kAny, VKEY_X}};

// U+0173 u ogonek
const PrintableSubEntry kU0173[] = {
    {DomCode::DIGIT7, false, false, kAny, kAny, VKEY_7},
    {DomCode::SEMICOLON, true, true, 0x0172, 0x0162,
     VKEY_OEM_1},  // U ogo., T ced.
    {DomCode::SEMICOLON, true, false, 0x0172, kAny, VKEY_OEM_3}};  // U ogonek

// U+017C z dot above
const PrintableSubEntry kU017C[] = {
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_OEM_4},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_5}};

// U+017E z caron
const PrintableSubEntry kU017E[] = {
    {DomCode::DIGIT6, false, false, kAny, kAny, VKEY_6},
    {DomCode::EQUAL, false, false, kAny, kAny, VKEY_OEM_PLUS},
    {DomCode::US_W, false, false, kAny, kAny, VKEY_W},
    {DomCode::BRACKET_LEFT, false, false, kAny, kAny, VKEY_Y},
    {DomCode::BACKSLASH, false, false, kAny, kAny, VKEY_OEM_5}};

// Table mapping unshifted characters to PrintableSubEntry tables.
struct PrintableMultiEntry {
  char16_t plain_character;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION const PrintableSubEntry* subtable;
  size_t subtable_size;
};

// Entries are ordered by character value.
const PrintableMultiEntry kMultiMap[] = {
    {0x0021, kU0021, std::size(kU0021)},  // exclamation mark
    {0x0022, kU0022, std::size(kU0022)},  // quotation mark
    {0x0023, kU0023, std::size(kU0023)},  // number sign
    {0x0024, kU0024, std::size(kU0024)},  // dollar sign
    {0x0027, kU0027, std::size(kU0027)},  // apostrophe
    {0x0028, kU0028, std::size(kU0028)},  // left parenthesis
    {0x0029, kU0029, std::size(kU0029)},  // right parenthesis
    {0x002A, kU002A, std::size(kU002A)},  // asterisk
    {0x002B, kU002B, std::size(kU002B)},  // plus sign
    {0x002C, kU002C, std::size(kU002C)},  // comma
    {0x002D, kU002D, std::size(kU002D)},  // hyphen-minus
    {0x002E, kU002E, std::size(kU002E)},  // full stop
    {0x002F, kU002F, std::size(kU002F)},  // solidus
    {0x003A, kU003A, std::size(kU003A)},  // colon
    {0x003B, kU003B, std::size(kU003B)},  // semicolon
    {0x003D, kU003D, std::size(kU003D)},  // equals sign
    {0x003F, kU003F, std::size(kU003F)},  // question mark
    {0x0040, kU0040, std::size(kU0040)},  // commercial at
    {0x005B, kU005B, std::size(kU005B)},  // left square bracket
    {0x005C, kU005C, std::size(kU005C)},  // reverse solidus
    {0x005D, kU005D, std::size(kU005D)},  // right square bracket
    {0x005F, kU005F, std::size(kU005F)},  // low line
    {0x0060, kU0060, std::size(kU0060)},  // grave accent
    {0x00A7, kU00A7, std::size(kU00A7)},  // section sign
    {0x00AB, kU00AB, std::size(kU00AB)},  // left double angle quotation mark
    {0x00B0, kU00B0, std::size(kU00B0)},  // degree sign
    {0x00BA, kU00BA, std::size(kU00BA)},  // masculine ordinal indicator
    {0x00E0, kU00E0, std::size(kU00E0)},  // a grave
    {0x00E1, kU00E1, std::size(kU00E1)},  // a acute
    {0x00E2, kU00E2, std::size(kU00E2)},  // a circumflex
    {0x00E4, kU00E4, std::size(kU00E4)},  // a diaeresis
    {0x00E6, kU00E6, std::size(kU00E6)},  // ae
    {0x00E7, kU00E7, std::size(kU00E7)},  // c cedilla
    {0x00E8, kU00E8, std::size(kU00E8)},  // e grave
    {0x00E9, kU00E9, std::size(kU00E9)},  // e acute
    {0x00ED, kU00ED, std::size(kU00ED)},  // i acute
    {0x00F0, kU00F0, std::size(kU00F0)},  // eth
    {0x00F3, kU00F3, std::size(kU00F3)},  // o acute
    {0x00F4, kU00F4, std::size(kU00F4)},  // o circumflex
    {0x00F6, kU00F6, std::size(kU00F6)},  // o diaeresis
    {0x00F8, kU00F8, std::size(kU00F8)},  // o stroke
    {0x00F9, kU00F9, std::size(kU00F9)},  // u grave
    {0x00FA, kU00FA, std::size(kU00FA)},  // u acute
    {0x00FC, kU00FC, std::size(kU00FC)},  // u diaeresis
    {0x0103, kU0103, std::size(kU0103)},  // a breve
    {0x0105, kU0105, std::size(kU0105)},  // a ogonek
    {0x010D, kU010D, std::size(kU010D)},  // c caron
    {0x0111, kU0111, std::size(kU0111)},  // d stroke
    {0x0117, kU0117, std::size(kU0117)},  // e dot above
    {0x0119, kU0119, std::size(kU0119)},  // e ogonek
    {0x012F, kU012F, std::size(kU012F)},  // i ogonek
    {0x0142, kU0142, std::size(kU0142)},  // l stroke
    {0x015F, kU015F, std::size(kU015F)},  // s cedilla
    {0x0161, kU0161, std::size(kU0161)},  // s caron
    {0x016B, kU016B, std::size(kU016B)},  // u macron
    {0x0173, kU0173, std::size(kU0173)},  // u ogonek
    {0x017C, kU017C, std::size(kU017C)},  // z dot above
    {0x017E, kU017E, std::size(kU017E)},  // z caron
};

// Table mapping unshifted characters to VKEY values.
struct PrintableSimpleEntry {
  char16_t plain_character;
  KeyboardCode key_code;
};

// Entries are ordered by character value.
const PrintableSimpleEntry kSimpleMap[] = {
    {0x0025, VKEY_5},          // percent sign
    {0x0026, VKEY_1},          // ampersand
    {0x003C, VKEY_OEM_5},      // less-than sign
    {0x007B, VKEY_OEM_7},      // left curly bracket
    {0x007C, VKEY_OEM_5},      // vertical line
    {0x007D, VKEY_OEM_2},      // right curly bracket
    {0x007E, VKEY_OEM_5},      // tilde
    {0x00A1, VKEY_OEM_6},      // inverted exclamation mark
    {0x00AD, VKEY_OEM_3},      // soft hyphen
    {0x00B2, VKEY_OEM_7},      // superscript two
    {0x00B5, VKEY_OEM_5},      // micro sign
    {0x00BB, VKEY_9},          // right-pointing double angle quotation mark
    {0x00BD, VKEY_OEM_5},      // vulgar fraction one half
    {0x00BF, VKEY_OEM_6},      // inverted question mark
    {0x00DF, VKEY_OEM_4},      // sharp s
    {0x00E5, VKEY_OEM_6},      // a ring above
    {0x00EA, VKEY_3},          // e circumflex
    {0x00EB, VKEY_OEM_1},      // e diaeresis
    {0x00EC, VKEY_OEM_6},      // i grave
    {0x00EE, VKEY_OEM_6},      // i circumflex
    {0x00F1, VKEY_OEM_3},      // n tilde
    {0x00F2, VKEY_OEM_3},      // o grave
    {0x00F5, VKEY_OEM_4},      // o tilde
    {0x00F7, VKEY_OEM_6},      // division sign
    {0x00FD, VKEY_7},          // y acute
    {0x00FE, VKEY_OEM_MINUS},  // thorn
    {0x0101, VKEY_OEM_8},      // a macron
    {0x0107, VKEY_OEM_7},      // c acute
    {0x010B, VKEY_OEM_3},      // c dot above
    {0x0113, VKEY_W},          // e macron
    {0x011B, VKEY_2},          // e caron
    {0x011F, VKEY_OEM_6},      // g breve
    {0x0121, VKEY_OEM_4},      // g dot above
    {0x0127, VKEY_OEM_6},      // h stroke
    {0x012B, VKEY_OEM_6},      // i macron
    {0x0131, VKEY_OEM_1},      // dotless i
    {0x0137, VKEY_OEM_5},      // k cedilla
    {0x013C, VKEY_OEM_2},      // l cedilla
    {0x013E, VKEY_2},          // l caron
    {0x0146, VKEY_OEM_4},      // n cedilla
    {0x0148, VKEY_OEM_5},      // n caron
    {0x0151, VKEY_OEM_4},      // o double acute
    {0x0159, VKEY_5},          // r caron
    {0x0163, VKEY_OEM_7},      // t cedilla
    {0x0165, VKEY_5},          // t caron
    {0x016F, VKEY_OEM_1},      // u ring above
    {0x0171, VKEY_OEM_5},      // u double acute
    {0x01A1, VKEY_OEM_6},      // o horn
    {0x01B0, VKEY_OEM_4},      // u horn
    {0x01B6, VKEY_OEM_6},      // z stroke
    {0x0259, VKEY_OEM_3},      // schwa
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
void LoadKeymap(const std::string& layout_name,
                scoped_refptr<base::SingleThreadTaskRunner> reply_runner,
                LoadKeymapCallback reply_callback) {
  std::string layout_id;
  std::string layout_variant;
  XkbKeyboardLayoutEngine::ParseLayoutName(layout_name, &layout_id,
                                           &layout_variant);
  xkb_rule_names names = {.rules = NULL,
                          .model = "pc101",
                          .layout = layout_id.c_str(),
                          .variant = layout_variant.c_str(),
                          .options = ""};
  std::unique_ptr<xkb_context, XkbContextDeleter> context;
  context.reset(xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES));
  xkb_context_include_path_append(context.get(), "/usr/share/X11/xkb");
  std::unique_ptr<xkb_keymap, XkbKeymapDeleter> keymap;
  keymap.reset(xkb_keymap_new_from_names(context.get(), &names,
                                         XKB_KEYMAP_COMPILE_NO_FLAGS));
  if (keymap) {
    std::unique_ptr<char, base::FreeDeleter> keymap_str(
        xkb_keymap_get_as_string(keymap.get(), XKB_KEYMAP_FORMAT_TEXT_V1));
    reply_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(reply_callback), layout_name,
                                  std::move(keymap_str)));
  } else {
    LOG(FATAL) << "Keymap file failed to load: " << layout_name;
  }
}
#endif

bool IsControlCharacter(uint32_t character) {
  return (character < 0x20) || (character > 0x7E && character < 0xA0);
}

}  // anonymous namespace

XkbKeyCodeConverter::XkbKeyCodeConverter() {
}

XkbKeyCodeConverter::~XkbKeyCodeConverter() {
}

XkbKeyboardLayoutEngine::XkbKeyboardLayoutEngine(
    const XkbKeyCodeConverter& converter)
    : key_code_converter_(converter), weak_ptr_factory_(this) {
  // TODO: add XKB_CONTEXT_NO_ENVIRONMENT_NAMES
  xkb_context_.reset(xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES));
  xkb_context_include_path_append(xkb_context_.get(),
                                  "/usr/share/X11/xkb");
}

XkbKeyboardLayoutEngine::~XkbKeyboardLayoutEngine() {
  for (const auto& entry : xkb_keymaps_) {
    xkb_keymap_unref(entry.keymap);
  }
}

std::string_view XkbKeyboardLayoutEngine::GetLayoutName() const {
  return current_layout_name_;
}

bool XkbKeyboardLayoutEngine::CanSetCurrentLayout() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif
}

void XkbKeyboardLayoutEngine::SetCurrentLayoutByName(
    const std::string& layout_name,
    base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  current_layout_name_ = layout_name;
  for (const auto& entry : xkb_keymaps_) {
    if (entry.layout_name == layout_name) {
      SetKeymap(entry.keymap);
      std::move(callback).Run(true);
      return;
    }
  }
  LoadKeymapCallback reply_callback =
      base::BindOnce(&XkbKeyboardLayoutEngine::OnKeymapLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&LoadKeymap, layout_name,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     std::move(reply_callback)));
#else
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void XkbKeyboardLayoutEngine::OnKeymapLoaded(
    base::OnceCallback<void(bool)> callback,
    const std::string& layout_name,
    std::unique_ptr<char, base::FreeDeleter> keymap_str) {
  if (keymap_str) {
    xkb_keymap* keymap = xkb_keymap_new_from_string(
        xkb_context_.get(), keymap_str.get(), XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS);
    XkbKeymapEntry entry = {layout_name, keymap};
    xkb_keymaps_.push_back(entry);
    if (layout_name == current_layout_name_) {
      SetKeymap(keymap);
      std::move(callback).Run(true);
    } else {
      std::move(callback).Run(false);
    }
  } else {
    LOG(FATAL) << "Keymap file failed to load: " << layout_name;
  }
}

bool XkbKeyboardLayoutEngine::UsesISOLevel5Shift() const {
  // NOTIMPLEMENTED();
  return false;
}

bool XkbKeyboardLayoutEngine::UsesAltGr() const {
  // NOTIMPLEMENTED();
  return false;
}

bool XkbKeyboardLayoutEngine::Lookup(DomCode dom_code,
                                     int flags,
                                     DomKey* dom_key,
                                     KeyboardCode* key_code) const {
  if (dom_code == DomCode::NONE)
    return false;
  // Convert DOM physical key to XKB representation.
  xkb_keycode_t xkb_keycode =
      key_code_converter_->DomCodeToXkbKeyCode(dom_code);
  if (xkb_keycode == key_code_converter_->InvalidXkbKeyCode()) {
    LOG(ERROR) << "No XKB keycode for DomCode 0x" << std::hex
               << static_cast<int>(dom_code) << " '"
               << KeycodeConverter::DomCodeToCodeString(dom_code) << "'";
    return false;
  }
  xkb_mod_mask_t xkb_flags = EventFlagsToXkbFlags(flags);
  // Obtain keysym and character.
  xkb_keysym_t xkb_keysym;
  uint32_t character = 0;
  if (!XkbLookup(xkb_keycode, xkb_flags, &xkb_keysym, &character)) {
    // If we do not have matching legacy Xkb keycode for the Dom code,
    // we could be dealing with a newer application launcher or similar
    // key. Let's see if we have a basic mapping for it.
    if (!DomCodeToNonPrintableDomKey(dom_code, dom_key, key_code)) {
      // If no Dom Key was found see if there is a matching Dom Code to keycode
      // mapping for it.
      *dom_key = DomKey::UNIDENTIFIED;
      *key_code = DomCodeToUsLayoutNonLocatedKeyboardCode(dom_code);
    }
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // XbdLookup conflates KEY_PRINT and KEY_SYSRQ (printscreen) by
  // mapping them both to XKB_KEY_Print rather than mapping KEY_SYSRQ to
  // XKB_KEY_3270_PrintScreen. This has become expected behavior on Linux,
  // but now ChromeOS can and wants to handle these keys separately.
  //
  // In the past in crbug/683097 both XKB keys were mapped to
  // DomKey::PRINT_SCREEN in keyboard_code_conversion_xkb.cc which has also
  // now been undone for ChromeOS only (not Linux)
  //
  // ChromeOS already correctly mapped the DomCode::PRINT_SCREEN and
  // DomCode::PRINT keys, but the lookup via XKB caused the incorrect
  // DomKey and subsequently incorrect VKEY to be used.
  //
  // This special cases this single key for ChromeOS platform, so that the
  // two keys behave as intended as below.
  //
  // KEY_PRINT > DomCode::PRINT > XKB_KEY_Print >
  //             DomKey::PRINT > VKEY_PRINT
  //
  // KEY_SYSRQ > DomCode::PRINT_SCREEN > XKB_KEY_3270_PrintScreen >
  //             DomKey::PRINT_SCREEN > VKEY_SNAPSHOT
  if (dom_code == DomCode::PRINT_SCREEN && xkb_keysym == XKB_KEY_Print) {
    xkb_keysym = XKB_KEY_3270_PrintScreen;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Classify the keysym and convert to DOM and VKEY representations.
  if (dom_code != DomCode::DIGIT2 || (flags & EF_CONTROL_DOWN) == 0) {
    // Non-character key. (We only support NUL as ^@ and ^2.)
    *dom_key = NonPrintableXKeySymToDomKey(xkb_keysym);
    if (*dom_key != DomKey::NONE) {
      *key_code = NonPrintableDomKeyToKeyboardCode(*dom_key);
      if (*key_code == VKEY_UNKNOWN)
        *key_code = DomCodeToUsLayoutNonLocatedKeyboardCode(dom_code);
      return true;
    }
    if (character == 0) {
      *dom_key = DomKey::UNIDENTIFIED;
      *key_code = DomCodeToUsLayoutNonLocatedKeyboardCode(dom_code);
      return true;
    }
  }

  // Per UI Events rules for determining |key|, if the character is
  // non-printable and a non-shiftlike modifier is down, we preferentially
  // return a printable key as if the modifier were not down.
  // https://w3c.github.io/uievents/#keys-guidelines
  const int kNonShiftlikeModifiers =
      EF_CONTROL_DOWN | EF_ALT_DOWN | EF_COMMAND_DOWN;
  if ((flags & kNonShiftlikeModifiers) && IsControlCharacter(character)) {
    int normal_ui_flags = flags & ~kNonShiftlikeModifiers;
    xkb_mod_mask_t normal_xkb_flags = EventFlagsToXkbFlags(normal_ui_flags);
    xkb_keysym_t normal_keysym;
    uint32_t normal_character = 0;
    if (XkbLookup(xkb_keycode, normal_xkb_flags, &normal_keysym,
                  &normal_character) &&
        !IsControlCharacter(normal_character)) {
      flags = normal_ui_flags;
      xkb_flags = normal_xkb_flags;
      character = normal_character;
      xkb_keysym = normal_keysym;
    }
  }

  *dom_key = DomKey::FromCharacter(character);
  *key_code = AlphanumericKeyboardCode(xkb_keysym, character);
  if (*key_code == VKEY_UNKNOWN) {
    *key_code = DifficultKeyboardCode(dom_code, flags, xkb_keycode, xkb_flags,
                                      xkb_keysym, character);
    if (*key_code == VKEY_UNKNOWN)
      *key_code = DomCodeToUsLayoutNonLocatedKeyboardCode(dom_code);
  }
  return true;
}

void XkbKeyboardLayoutEngine::SetInitCallbackForTest(
    base::OnceClosure closure) {
  if (xkb_state_) {
    std::move(closure).Run();
    return;
  }

  keymap_init_closure_for_test_ = std::move(closure);
}

bool XkbKeyboardLayoutEngine::SetCurrentLayoutFromBuffer(
    const char* keymap_string,
    size_t size) {
  xkb_keymap* keymap = xkb_keymap_new_from_buffer(
      xkb_context_.get(), keymap_string, size, XKB_KEYMAP_FORMAT_TEXT_V1,
      XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!keymap)
    return false;

  SetKeymap(keymap);

  // Store the keymap that will be unrefed either on dtor or if a new keymap is
  // created.
  key_map_from_buffer_.reset(keymap);
  return true;
}

void XkbKeyboardLayoutEngine::SetKeymap(xkb_keymap* keymap) {
  xkb_state_.reset(xkb_state_new(keymap));
  xkb_modifier_converter_ = XkbModifierConverter::CreateFromKeymap(keymap);
  shift_mod_mask_ = xkb_modifier_converter_.MaskFromUiFlags(ui::EF_SHIFT_DOWN);
  altgr_mod_mask_ = xkb_modifier_converter_.MaskFromUiFlags(ui::EF_ALTGR_DOWN);

  // Reconstruct keysym map.
  std::vector<XkbKeysymMapEntry> keysym_map;

  const xkb_keycode_t min_key = xkb_keymap_min_keycode(keymap);
  const xkb_keycode_t max_key = xkb_keymap_max_keycode(keymap);
  for (xkb_keycode_t keycode = min_key; keycode <= max_key; ++keycode) {
    const xkb_layout_index_t num_layouts =
        xkb_keymap_num_layouts_for_key(keymap, keycode);
    for (xkb_layout_index_t layout = 0; layout < num_layouts; ++layout) {
      const xkb_level_index_t num_levels =
          xkb_keymap_num_levels_for_key(keymap, keycode, layout);
      for (xkb_level_index_t level = 0; level < num_levels; ++level) {
        const xkb_keysym_t* keysyms;
        int num_syms = xkb_keymap_key_get_syms_by_level(keymap, keycode, layout,
                                                        level, &keysyms);
        for (int i = 0; i < num_syms; ++i)
          keysym_map.emplace_back(
              XkbKeysymMapEntry{keysyms[i], keycode, layout});
      }
    }
  }

  // Then sort and unique here. On tie break, smaller keycode comes first.
  std::sort(
      keysym_map.begin(), keysym_map.end(),
      [](const XkbKeysymMapEntry& entry1, const XkbKeysymMapEntry& entry2) {
        return std::tie(entry1.xkb_keysym, entry1.xkb_keycode,
                        entry1.xkb_layout) < std::tie(entry2.xkb_keysym,
                                                      entry2.xkb_keycode,
                                                      entry2.xkb_layout);
      });
  keysym_map.erase(
      std::unique(
          keysym_map.begin(), keysym_map.end(),
          [](const XkbKeysymMapEntry& entry1, const XkbKeysymMapEntry& entry2) {
            return std::tie(entry1.xkb_keysym, entry1.xkb_keycode,
                            entry1.xkb_layout) == std::tie(entry2.xkb_keysym,
                                                           entry2.xkb_keycode,
                                                           entry2.xkb_layout);
          }),
      keysym_map.end());
  xkb_keysym_map_ = std::move(keysym_map);

  layout_index_ = 0;
  if (keymap_init_closure_for_test_)
    std::move(keymap_init_closure_for_test_).Run();
}

xkb_mod_mask_t XkbKeyboardLayoutEngine::EventFlagsToXkbFlags(
    int ui_flags) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // In ChromeOS NumLock is always on.
  ui_flags |= ui::EF_NUM_LOCK_ON;
#endif
  return xkb_modifier_converter_.MaskFromUiFlags(ui_flags);
}

int XkbKeyboardLayoutEngine::UpdateModifiers(uint32_t depressed,
                                             uint32_t latched,
                                             uint32_t locked,
                                             uint32_t group) {
  auto* state = xkb_state_.get();
  xkb_state_update_mask(state, depressed, latched, locked, 0, 0, group);
  auto component = static_cast<xkb_state_component>(XKB_STATE_MODS_DEPRESSED |
                                                    XKB_STATE_MODS_LATCHED |
                                                    XKB_STATE_MODS_LOCKED);
  xkb_mod_index_t num_mods =
      xkb_keymap_num_mods(xkb_state_get_keymap(xkb_state_.get()));
  xkb_mod_mask_t mask = 0;
  for (xkb_mod_index_t i = 0; i < num_mods; ++i) {
    if (xkb_state_mod_index_is_active(state, i, component))
      mask |= (1 << i);
  }
  layout_index_ = group;
  return xkb_modifier_converter_.UiFlagsFromMask(mask);
}

DomCode XkbKeyboardLayoutEngine::GetDomCodeByKeysym(
    uint32_t keysym,
    const std::optional<std::vector<std::string_view>>& modifiers) const {
  // Look up all candidates.
  auto range = std::equal_range(
      xkb_keysym_map_.begin(), xkb_keysym_map_.end(), XkbKeysymMapEntry{keysym},
      [](const XkbKeysymMapEntry& entry1, const XkbKeysymMapEntry& entry2) {
        return entry1.xkb_keysym < entry2.xkb_keysym;
      });
  if (range.first != range.second) {
    // If modifier is not given, use the first entry, which is smallest keycode.
    // This is just for backward compatibility.
    if (!modifiers.has_value())
      return KeycodeConverter::NativeKeycodeToDomCode(range.first->xkb_keycode);
    xkb_mod_mask_t xkb_modifiers =
        xkb_modifier_converter_.MaskFromNames(*modifiers);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // In ChromeOS NumLock is always on.
    xkb_modifiers |=
        xkb_modifier_converter_.MaskFromUiFlags(ui::EF_NUM_LOCK_ON);
#endif
    // Note: value is already in the lexicographical order, so smaller keycode
    // comes first.
    for (std::unique_ptr<xkb_state, XkbStateDeleter> xkb_state(
             xkb_state_new(xkb_state_get_keymap(xkb_state_.get())));
         range.first != range.second; ++range.first) {
      xkb_keycode_t xkb_keycode = range.first->xkb_keycode;
      xkb_layout_index_t xkb_layout = range.first->xkb_layout;
      // The argument does not have any info about the layout, so we assume the
      // current layout here.
      if (xkb_layout != layout_index_)
        continue;
      xkb_state_update_mask(xkb_state.get(), xkb_modifiers, 0, 0, 0, 0,
                            xkb_layout);
      const xkb_keysym_t* out_keysyms;
      int num_syms =
          xkb_state_key_get_syms(xkb_state.get(), xkb_keycode, &out_keysyms);
      for (int i = 0; i < num_syms; ++i) {
        if (out_keysyms[i] == keysym)
          return KeycodeConverter::NativeKeycodeToDomCode(xkb_keycode);
      }
    }
  }

  VLOG(1) << "No Keycode found for the keysym: " << keysym << ", modifiers: "
          << (modifiers.has_value() ? base::JoinString(modifiers.value(), ",")
                                    : "(no modifiers)");
  return DomCode::NONE;
}

bool XkbKeyboardLayoutEngine::XkbLookup(xkb_keycode_t xkb_keycode,
                                        xkb_mod_mask_t xkb_flags,
                                        xkb_keysym_t* xkb_keysym,
                                        uint32_t* character) const {
  if (!xkb_state_) {
    LOG(ERROR) << "No current XKB state";
    return false;
  }

  auto* state = xkb_state_.get();
  xkb_state_update_mask(state, xkb_flags, 0, 0, 0, 0, layout_index_);
  *xkb_keysym = xkb_state_key_get_one_sym(state, xkb_keycode);

  if (*xkb_keysym == XKB_KEY_NoSymbol)
    return false;
  *character = xkb_state_key_get_utf32(state, xkb_keycode);
  return true;
}

KeyboardCode XkbKeyboardLayoutEngine::DifficultKeyboardCode(
    DomCode dom_code,
    int ui_flags,
    xkb_keycode_t xkb_keycode,
    xkb_mod_mask_t xkb_flags,
    xkb_keysym_t xkb_keysym,
    char16_t character) const {
  // Get the layout interpretation without modifiers, so that
  // e.g. Ctrl+D correctly generates VKEY_D.
  xkb_keysym_t plain_keysym;
  uint32_t plain_character;
  if (!XkbLookup(xkb_keycode, 0, &plain_keysym, &plain_character))
    return VKEY_UNKNOWN;

  // If the plain key is non-printable, that determines the VKEY.
  DomKey plain_key = NonPrintableXKeySymToDomKey(plain_keysym);
  if (plain_key != ui::DomKey::NONE)
    return NonPrintableDomKeyToKeyboardCode(plain_key);

  // Plain ASCII letters and digits map directly to VKEY values.
  KeyboardCode key_code = AlphanumericKeyboardCode(xkb_keysym, plain_character);
  if (key_code != VKEY_UNKNOWN)
    return key_code;

  // Check the multi-character tables.
  const PrintableMultiEntry* multi_end = kMultiMap + std::size(kMultiMap);
  const PrintableMultiEntry* multi =
      std::lower_bound(kMultiMap, multi_end, plain_character,
                       [](const PrintableMultiEntry& e, char16_t c) {
                         return e.plain_character < c;
                       });
  if ((multi != multi_end) && (multi->plain_character == plain_character)) {
    const char16_t kNonCharacter = kAny;
    char16_t shift_character = kNonCharacter;
    char16_t altgr_character = kNonCharacter;
    for (size_t i = 0; i < multi->subtable_size; ++i) {
      if (multi->subtable[i].dom_code != dom_code)
        continue;
      if (multi->subtable[i].test_shift) {
        if (shift_character == kNonCharacter) {
          shift_character = XkbSubCharacter(xkb_keycode, xkb_flags, character,
                                            shift_mod_mask_);
        }
        if (shift_character != multi->subtable[i].shift_character)
          continue;
      }
      if (multi->subtable[i].test_altgr) {
        if (altgr_character == kNonCharacter) {
          altgr_character = XkbSubCharacter(xkb_keycode, xkb_flags, character,
                                            altgr_mod_mask_);
        }
        if (altgr_character != multi->subtable[i].altgr_character)
          continue;
      }
      return multi->subtable[i].key_code;
    }
  }

  // Check the simple character table.
  const PrintableSimpleEntry* simple_end = kSimpleMap + std::size(kSimpleMap);
  const PrintableSimpleEntry* simple =
      std::lower_bound(kSimpleMap, simple_end, plain_character,
                       [](const PrintableSimpleEntry& e, char16_t c) {
                         return e.plain_character < c;
                       });
  if ((simple != simple_end) && (simple->plain_character == plain_character))
    return simple->key_code;

  return VKEY_UNKNOWN;
}

char16_t XkbKeyboardLayoutEngine::XkbSubCharacter(xkb_keycode_t xkb_keycode,
                                                  xkb_mod_mask_t base_flags,
                                                  char16_t base_character,
                                                  xkb_mod_mask_t flags) const {
  if (flags == base_flags)
    return base_character;
  xkb_keysym_t keysym;
  uint32_t character = 0;
  if (!XkbLookup(xkb_keycode, flags, &keysym, &character))
    character = kNone;
  return character;
}

void XkbKeyboardLayoutEngine::ParseLayoutName(const std::string& layout_name,
                                              std::string* layout_id,
                                              std::string* layout_variant) {
  size_t dash_index = layout_name.find('-');
  size_t parentheses_index = layout_name.find('(');
  *layout_id = layout_name;
  *layout_variant = "";
  if (parentheses_index != std::string::npos) {
    *layout_id = layout_name.substr(0, parentheses_index);
    size_t close_index = layout_name.find(')', parentheses_index);
    if (close_index == std::string::npos)
      close_index = layout_name.size();
    *layout_variant = layout_name.substr(parentheses_index + 1,
                                         close_index - parentheses_index - 1);
  } else if (dash_index != std::string::npos) {
    *layout_id = layout_name.substr(0, dash_index);
    *layout_variant = layout_name.substr(dash_index + 1);
  }
}

}  // namespace ui
