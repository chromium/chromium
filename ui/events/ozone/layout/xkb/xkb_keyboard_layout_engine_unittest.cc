// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"

#include <stddef.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon-names.h>

#include <string_view>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/scoped_keyboard_layout_engine.h"

namespace ui {

namespace {

// This XkbKeyCodeConverter simply uses the numeric value of the DomCode.
class VkTestXkbKeyCodeConverter : public XkbKeyCodeConverter {
 public:
  VkTestXkbKeyCodeConverter() {
    invalid_xkb_keycode_ = static_cast<xkb_keycode_t>(DomCode::NONE);
  }
  ~VkTestXkbKeyCodeConverter() override {}
  xkb_keycode_t DomCodeToXkbKeyCode(DomCode dom_code) const override {
    return static_cast<xkb_keycode_t>(dom_code);
  }
};

// This mock XkbKeyboardLayoutEngine fakes a layout that succeeds only when the
// supplied keycode matches its |Entry|, in which case it supplies DomKey::NONE
// and a character that depends on the flags.
class VkTestXkbKeyboardLayoutEngine : public XkbKeyboardLayoutEngine {
 public:
  enum class EntryType { NONE, PRINTABLE, KEYSYM };
  struct PrintableEntry {
    char16_t plain_character;
    char16_t shift_character;
    char16_t altgr_character;
    DomCode dom_code;
  };
  struct KeysymEntry {
    DomCode dom_code;
    int flags;
    xkb_keysym_t keysym;
    char16_t character;
  };

  struct RuleNames {
    std::string layout_name;
    std::string layout;
    std::string variant;
  };

 public:
  VkTestXkbKeyboardLayoutEngine(const XkbKeyCodeConverter& keycode_converter)
      : XkbKeyboardLayoutEngine(keycode_converter),
        entry_type_(EntryType::NONE),
        printable_entry_(nullptr) {
    // For testing, use the same modifier values as ui::EventFlags.
    xkb_modifier_converter_ = XkbModifierConverter({
        "",
        XKB_MOD_NAME_SHIFT,
        XKB_MOD_NAME_CTRL,
        XKB_MOD_NAME_ALT,
        XKB_MOD_NAME_LOGO,
        "",
        "Mod5",
        "Mod3",
        XKB_MOD_NAME_CAPS,
        XKB_MOD_NAME_NUM,
    });
    shift_mod_mask_ = xkb_modifier_converter_.MaskFromUiFlags(EF_SHIFT_DOWN);
    altgr_mod_mask_ = xkb_modifier_converter_.MaskFromUiFlags(EF_ALTGR_DOWN);
  }
  ~VkTestXkbKeyboardLayoutEngine() override = default;

  void SetEntry(const PrintableEntry* entry) {
    entry_type_ = EntryType::PRINTABLE;
    printable_entry_ = entry;
  }

  void SetEntry(const KeysymEntry* entry) {
    entry_type_ = EntryType::KEYSYM;
    keysym_entry_ = entry;
  }

  xkb_keysym_t CharacterToKeySym(char16_t c) const { return 0x01000000u + c; }

  KeyboardCode GetKeyboardCode(DomCode dom_code,
                               int flags,
                               char16_t character) const {
    KeyboardCode key_code = DifficultKeyboardCode(
        dom_code, flags, key_code_converter_->DomCodeToXkbKeyCode(dom_code),
        flags, CharacterToKeySym(character), character);
    if (key_code == VKEY_UNKNOWN) {
      DomKey dummy_dom_key;
      // If this fails, key_code remains VKEY_UNKNOWN.
      std::ignore =
          DomCodeToUsLayoutDomKey(dom_code, EF_NONE, &dummy_dom_key, &key_code);
    }
    return key_code;
  }

  // XkbKeyboardLayoutEngine overrides:
  bool XkbLookup(xkb_keycode_t xkb_keycode,
                 xkb_mod_mask_t xkb_flags,
                 xkb_keysym_t* xkb_keysym,
                 uint32_t* character) const override {
    switch (entry_type_) {
      case EntryType::NONE:
        break;
      case EntryType::PRINTABLE:
        if (!printable_entry_ ||
            (xkb_keycode !=
             static_cast<xkb_keycode_t>(printable_entry_->dom_code))) {
          return false;
        }
        if (xkb_flags & EF_ALTGR_DOWN)
          *character = printable_entry_->altgr_character;
        else if (xkb_flags & EF_SHIFT_DOWN)
          *character = printable_entry_->shift_character;
        else
          *character = printable_entry_->plain_character;
        *xkb_keysym = CharacterToKeySym(*character);
        return *character != 0;
      case EntryType::KEYSYM:
        if (!keysym_entry_ ||
            (xkb_keycode !=
             static_cast<xkb_keycode_t>(keysym_entry_->dom_code))) {
          return false;
        }
        *xkb_keysym = keysym_entry_->keysym;
        *character = keysym_entry_->character;
        return true;
    }
    return false;
  }

 private:
  EntryType entry_type_;
  raw_ptr<const PrintableEntry> printable_entry_;
  raw_ptr<const KeysymEntry> keysym_entry_;
};

}  // anonymous namespace

class XkbLayoutEngineVkTest : public testing::Test {
 public:
  XkbLayoutEngineVkTest()
      : layout_engine_(std::make_unique<VkTestXkbKeyboardLayoutEngine>(
            keycode_converter_)) {}
  ~XkbLayoutEngineVkTest() override {}

 protected:
  VkTestXkbKeyCodeConverter keycode_converter_;
  std::unique_ptr<VkTestXkbKeyboardLayoutEngine> layout_engine_;
};

TEST_F(XkbLayoutEngineVkTest, KeyboardCodeForPrintable) {
  // This table contains U+2460 CIRCLED DIGIT ONE, U+2461 CIRCLED DIGIT TWO,
  // and DomCode::NONE where the result should not depend on those values.
  static const struct {
    VkTestXkbKeyboardLayoutEngine::PrintableEntry test;
    KeyboardCode key_code;
  } kVkeyTestCase[] = {
      // Cases requiring mapping tables.
      // exclamation mark, *, *
      /* 0 */ {{0x0021, 0x2460, 0x2461, DomCode::DIGIT1}, VKEY_1},
      // exclamation mark, *, *
      /* 1 */ {{0x0021, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // exclamation mark, *, *
      /* 2 */ {{0x0021, 0x2460, 0x2461, DomCode::SLASH}, VKEY_OEM_8},
      // quotation mark, *, *
      /* 3 */ {{0x0022, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_2},
      // quotation mark, *, *
      /* 4 */ {{0x0022, 0x2460, 0x2461, DomCode::DIGIT3}, VKEY_3},
      // number sign, apostrophe, *
      /* 5 */ {{0x0023, 0x0027, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_2},
      // number sign, tilde, unmapped
      /* 6 */ {{0x0023, 0x007E, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_7},
      // number sign, *, *
      /* 7 */ {{0x0023, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_7},
      // dollar sign, *, *
      /* 8 */ {{0x0024, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_8},
      // dollar sign, *, *
      /* 9 */ {{0x0024, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_1},
      // percent sign, *, *
      /* 10 */ {{0x0025, 0x2460, 0x2461, DomCode::NONE}, VKEY_5},
      // ampersand, *, *
      /* 11 */ {{0x0026, 0x2460, 0x2461, DomCode::NONE}, VKEY_1},
      // apostrophe, unmapped, *
      /* 12 */ {{0x0027, 0x0000, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // apostrophe, quotation mark, unmapped
      /* 13 */ {{0x0027, 0x0022, 0x2461, DomCode::US_Z}, VKEY_Z},
      // apostrophe, quotation mark, R caron
      /* 14 */ {{0x0027, 0x0022, 0x0158, DomCode::US_Z}, VKEY_OEM_7},
      // apostrophe, quotation mark, *
      /* 15 */ {{0x0027, 0x0022, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // apostrophe, quotation mark, *
      /* 16 */ {{0x0027, 0x0022, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // apostrophe, asterisk, unmapped
      /* 17 */ {{0x0027, 0x002A, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_2},
      // apostrophe, asterisk, unmapped
      /* 18 */ {{0x0027, 0x002A, 0x2461, DomCode::EQUAL}, VKEY_OEM_PLUS},
      // apostrophe, asterisk, vulgar fraction one half
      /* 19 */ {{0x0027, 0x002A, 0x00BD, DomCode::BACKSLASH}, VKEY_OEM_5},
      // apostrophe, asterisk, L stroke
      /* 20 */ {{0x0027, 0x002A, 0x0141, DomCode::BACKSLASH}, VKEY_OEM_2},
      // apostrophe, question mark, unmapped
      /* 21 */ {{0x0027, 0x003F, 0x2461, DomCode::MINUS}, VKEY_OEM_4},
      // apostrophe, question mark, Y acute
      /* 22 */ {{0x0027, 0x003F, 0x00DD, DomCode::MINUS}, VKEY_OEM_4},
      // apostrophe, commercial at, unmapped
      /* 23 */ {{0x0027, 0x0040, 0x2461, DomCode::QUOTE}, VKEY_OEM_3},
      // apostrophe, middle dot, *
      /* 24 */ {{0x0027, 0x00B7, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_5},
      // apostrophe, *, *
      /* 25 */ {{0x0027, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_1},
      // apostrophe, *, *
      /* 26 */ {{0x0027, 0x2460, 0x2461, DomCode::DIGIT4}, VKEY_4},
      // apostrophe, *, *
      /* 27 */ {{0x0027, 0x2460, 0x2461, DomCode::US_Q}, VKEY_OEM_7},
      // apostrophe, *, *
      /* 28 */ {{0x0027, 0x2460, 0x2461, DomCode::SLASH}, VKEY_OEM_7},
      // left parenthesis, *, *
      /* 29 */ {{0x0028, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // left parenthesis, *, *
      /* 30 */ {{0x0028, 0x2460, 0x2461, DomCode::DIGIT5}, VKEY_5},
      // left parenthesis, *, *
      /* 31 */ {{0x0028, 0x2460, 0x2461, DomCode::DIGIT9}, VKEY_9},
      // right parenthesis, *, *
      /* 32 */ {{0x0029, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // right parenthesis, *, *
      /* 33 */ {{0x0029, 0x2460, 0x2461, DomCode::DIGIT0}, VKEY_0},
      // right parenthesis, *, *
      /* 34 */ {{0x0029, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_4},
      // asterisk, *, *
      /* 35 */ {{0x002A, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // asterisk, *, *
      /* 36 */ {{0x002A, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_1},
      // plus sign, question mark, unmapped
      /* 37 */ {{0x002B, 0x003F, 0x2461, DomCode::MINUS}, VKEY_OEM_PLUS},
      // plus sign, question mark, reverse solidus
      /* 38 */ {{0x002B, 0x003F, 0x005C, DomCode::MINUS}, VKEY_OEM_PLUS},
      // plus sign, question mark, o double acute
      /* 39 */ {{0x002B, 0x003F, 0x0151, DomCode::MINUS}, VKEY_OEM_PLUS},
      // plus sign, *, *
      /* 40 */ {{0x002B, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_2},
      // plus sign, *, *
      /* 41 */ {{0x002B, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_PLUS},
      // plus sign, *, *
      /* 42 */
      {{0x002B, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_PLUS},
      // plus sign, *, *
      /* 43 */ {{0x002B, 0x2460, 0x2461, DomCode::DIGIT1}, VKEY_1},
      // plus sign, *, *
      /* 44 */ {{0x002B, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_PLUS},
      // plus sign, *, *
      /* 45 */ {{0x002B, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_PLUS},
      // comma, *, *
      /* 46 */ {{0x002C, 0x2460, 0x2461, DomCode::COMMA}, VKEY_OEM_COMMA},
      // comma, *, *
      /* 47 */ {{0x002C, 0x2460, 0x2461, DomCode::DIGIT3}, VKEY_3},
      // comma, *, *
      /* 48 */ {{0x002C, 0x2460, 0x2461, DomCode::DIGIT5}, VKEY_5},
      // comma, *, *
      /* 49 */ {{0x002C, 0x2460, 0x2461, DomCode::DIGIT6}, VKEY_6},
      // comma, *, *
      /* 50 */ {{0x002C, 0x2460, 0x2461, DomCode::DIGIT9}, VKEY_9},
      // comma, *, *
      /* 51 */ {{0x002C, 0x2460, 0x2461, DomCode::US_M}, VKEY_OEM_COMMA},
      // comma, *, *
      /* 52 */ {{0x002C, 0x2460, 0x2461, DomCode::US_V}, VKEY_OEM_COMMA},
      // comma, *, *
      /* 53 */ {{0x002C, 0x2460, 0x2461, DomCode::US_W}, VKEY_OEM_COMMA},
      // hyphen-minus, equals sign, *
      /* 54 */ {{0x002D, 0x003D, 0x2461, DomCode::SLASH}, VKEY_OEM_MINUS},
      // hyphen-minus, low line, unmapped
      /* 55 */ {{0x002D, 0x005F, 0x2461, DomCode::EQUAL}, VKEY_OEM_MINUS},
      // hyphen-minus, low line, unmapped
      /* 56 */ {{0x002D, 0x005F, 0x2461, DomCode::SLASH}, VKEY_OEM_MINUS},
      // hyphen-minus, low line, asterisk
      /* 57 */ {{0x002D, 0x005F, 0x002A, DomCode::SLASH}, VKEY_OEM_MINUS},
      // hyphen-minus, low line, solidus
      /* 58 */ {{0x002D, 0x005F, 0x002F, DomCode::SLASH}, VKEY_OEM_2},
      // hyphen-minus, low line, n
      /* 59 */ {{0x002D, 0x005F, 0x006E, DomCode::SLASH}, VKEY_OEM_MINUS},
      // hyphen-minus, low line, r cedilla
      /* 60 */ {{0x002D, 0x005F, 0x0157, DomCode::EQUAL}, VKEY_OEM_4},
      // hyphen-minus, *, *
      /* 61 */ {{0x002D, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_2},
      // hyphen-minus, *, *
      /* 62 */ {{0x002D, 0x2460, 0x2461, DomCode::DIGIT6}, VKEY_6},
      // hyphen-minus, *, *
      /* 63 */ {{0x002D, 0x2460, 0x2461, DomCode::US_A}, VKEY_OEM_MINUS},
      // hyphen-minus, *, *
      /* 64 */ {{0x002D, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_MINUS},
      // hyphen-minus, *, *
      /* 65 */ {{0x002D, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_MINUS},
      // full stop, *, *
      /* 66 */ {{0x002E, 0x2460, 0x2461, DomCode::DIGIT7}, VKEY_7},
      // full stop, *, *
      /* 67 */ {{0x002E, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // full stop, *, *
      /* 68 */ {{0x002E, 0x2460, 0x2461, DomCode::US_E}, VKEY_OEM_PERIOD},
      // full stop, *, *
      /* 69 */ {{0x002E, 0x2460, 0x2461, DomCode::US_O}, VKEY_OEM_PERIOD},
      // full stop, *, *
      /* 70 */ {{0x002E, 0x2460, 0x2461, DomCode::US_R}, VKEY_OEM_PERIOD},
      // full stop, *, *
      /* 71 */ {{0x002E, 0x2460, 0x2461, DomCode::PERIOD}, VKEY_OEM_PERIOD},
      // full stop, *, *
      /* 72 */ {{0x002E, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // full stop, *, *
      /* 73 */ {{0x002E, 0x2460, 0x2461, DomCode::SLASH}, VKEY_OEM_2},
      // solidus, digit zero, *
      /* 74 */ {{0x002F, 0x0030, 0x2461, DomCode::DIGIT0}, VKEY_0},
      // solidus, digit three, *
      /* 75 */ {{0x002F, 0x0033, 0x2461, DomCode::DIGIT3}, VKEY_3},
      // solidus, question mark, *
      /* 76 */ {{0x002F, 0x003F, 0x2461, DomCode::DIGIT0}, VKEY_OEM_2},
      // solidus, question mark, *
      /* 77 */ {{0x002F, 0x003F, 0x2461, DomCode::DIGIT3}, VKEY_OEM_2},
      // solidus, *, *
      /* 78 */ {{0x002F, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_7},
      // solidus, *, *
      /* 79 */ {{0x002F, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // solidus, *, *
      /* 80 */ {{0x002F, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_2},
      // solidus, *, *
      /* 81 */ {{0x002F, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_4},
      // solidus, *, *
      /* 82 */ {{0x002F, 0x2460, 0x2461, DomCode::SLASH}, VKEY_OEM_2},
      // solidus, *, *
      /* 83 */
      {{0x002F, 0x2460, 0x2461, DomCode::CONTROL_RIGHT}, VKEY_RCONTROL},
      // colon, *, *
      /* 84 */ {{0x003A, 0x2460, 0x2461, DomCode::DIGIT1}, VKEY_1},
      // colon, *, *
      /* 85 */ {{0x003A, 0x2460, 0x2461, DomCode::DIGIT5}, VKEY_5},
      // colon, *, *
      /* 86 */ {{0x003A, 0x2460, 0x2461, DomCode::DIGIT6}, VKEY_6},
      // colon, *, *
      /* 87 */ {{0x003A, 0x2460, 0x2461, DomCode::PERIOD}, VKEY_OEM_2},
      // semicolon, *, *
      /* 88 */ {{0x003B, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // semicolon, *, *
      /* 89 */ {{0x003B, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_1},
      // semicolon, *, *
      /* 90 */ {{0x003B, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // semicolon, *, *
      /* 91 */ {{0x003B, 0x2460, 0x2461, DomCode::COMMA}, VKEY_OEM_PERIOD},
      // semicolon, *, *
      /* 92 */ {{0x003B, 0x2460, 0x2461, DomCode::DIGIT4}, VKEY_4},
      // semicolon, *, *
      /* 93 */ {{0x003B, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // semicolon, *, *
      /* 94 */ {{0x003B, 0x2460, 0x2461, DomCode::US_Q}, VKEY_OEM_1},
      // semicolon, *, *
      /* 95 */ {{0x003B, 0x2460, 0x2461, DomCode::US_Z}, VKEY_OEM_1},
      // semicolon, *, *
      /* 96 */ {{0x003B, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // semicolon, *, *
      /* 97 */ {{0x003B, 0x2460, 0x2461, DomCode::SLASH}, VKEY_OEM_2},
      // less-than sign, *, *
      /* 98 */ {{0x003C, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // equals sign, percent sign, unmapped
      /* 99 */ {{0x003D, 0x0025, 0x2461, DomCode::MINUS}, VKEY_OEM_PLUS},
      // equals sign, percent sign, hyphen-minus
      /* 100 */ {{0x003D, 0x0025, 0x002D, DomCode::MINUS}, VKEY_OEM_MINUS},
      // equals sign, percent sign, *
      /* 101 */ {{0x003D, 0x0025, 0x2461, DomCode::SLASH}, VKEY_OEM_8},
      // equals sign, plus sign, *
      /* 102 */ {{0x003D, 0x002B, 0x2461, DomCode::SLASH}, VKEY_OEM_PLUS},
      // equals sign, *, *
      /* 103 */
      {{0x003D, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_PLUS},
      // equals sign, *, *
      /* 104 */ {{0x003D, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // equals sign, *, *
      /* 105 */ {{0x003D, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_PLUS},
      // question mark, *, *
      /* 106 */ {{0x003F, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_2},
      // question mark, *, *
      /* 107 */ {{0x003F, 0x2460, 0x2461, DomCode::DIGIT7}, VKEY_7},
      // question mark, *, *
      /* 108 */ {{0x003F, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // question mark, *, *
      /* 109 */ {{0x003F, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_PLUS},
      // commercial at, *, *
      /* 110 */ {{0x0040, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_7},
      // commercial at, *, *
      /* 111 */ {{0x0040, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // left square bracket, *, *
      /* 112 */ {{0x005B, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // left square bracket, *, *
      /* 113 */ {{0x005B, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // left square bracket, *, *
      /* 114 */ {{0x005B, 0x2460, 0x2461, DomCode::DIGIT1}, VKEY_OEM_4},
      // left square bracket, *, *
      /* 115 */ {{0x005B, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_4},
      // left square bracket, *, *
      /* 116 */ {{0x005B, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // reverse solidus, solidus, *
      /* 117 */ {{0x005C, 0x002F, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_7},
      // reverse solidus, vertical line, digit one
      /* 118 */ {{0x005C, 0x007C, 0x0031, DomCode::BACKQUOTE}, VKEY_OEM_5},
      // reverse solidus, vertical line, N cedilla
      /* 119 */ {{0x005C, 0x007C, 0x0145, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // reverse solidus, vertical line, *
      /* 120 */ {{0x005C, 0x007C, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // reverse solidus, *, *
      /* 121 */ {{0x005C, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_4},
      // right square bracket, *, *
      /* 122 */ {{0x005D, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // right square bracket, *, *
      /* 123 */ {{0x005D, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // right square bracket, *, *
      /* 124 */ {{0x005D, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // right square bracket, *, *
      /* 125 */ {{0x005D, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_OEM_6},
      // right square bracket, *, *
      /* 126 */ {{0x005D, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_6},
      // low line, *, *
      /* 127 */ {{0x005F, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // low line, *, *
      /* 128 */ {{0x005F, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_MINUS},
      // grave accent, unmapped, *
      /* 129 */ {{0x0060, 0x0000, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, tilde, unmapped
      /* 130 */ {{0x0060, 0x007E, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, tilde, digit one
      /* 131 */ {{0x0060, 0x007E, 0x0031, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, tilde, semicolon
      /* 132 */ {{0x0060, 0x007E, 0x003B, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, tilde, grave accent
      /* 133 */ {{0x0060, 0x007E, 0x0060, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, tilde, inverted question mark
      /* 134 */ {{0x0060, 0x007E, 0x00BF, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, tilde, o double acute
      /* 135 */ {{0x0060, 0x007E, 0x0151, DomCode::BACKQUOTE}, VKEY_OEM_3},
      // grave accent, not sign, *
      /* 136 */ {{0x0060, 0x00AC, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_8},
      // left curly bracket, *, *
      /* 137 */ {{0x007B, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_7},
      // vertical line, *, *
      /* 138 */ {{0x007C, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // right curly bracket, *, *
      /* 139 */ {{0x007D, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_2},
      // tilde, *, *
      /* 140 */ {{0x007E, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // inverted exclamation mark, *, *
      /* 141 */ {{0x00A1, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // section sign, degree sign, *
      /* 142 */ {{0x00A7, 0x00B0, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_2},
      // section sign, vulgar fraction one half, *
      /* 143 */ {{0x00A7, 0x00BD, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_5},
      // section sign, *, *
      /* 144 */ {{0x00A7, 0x2460, 0x2461, DomCode::DIGIT4}, VKEY_4},
      // section sign, *, *
      /* 145 */ {{0x00A7, 0x2460, 0x2461, DomCode::DIGIT6}, VKEY_6},
      // section sign, *, *
      /* 146 */ {{0x00A7, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // left-pointing double angle quotation mark, *, *
      /* 147 */ {{0x00AB, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // left-pointing double angle quotation mark, *, *
      /* 148 */ {{0x00AB, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_6},
      // soft hyphen, *, *
      /* 149 */ {{0x00AD, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_3},
      // degree sign, *, *
      /* 150 */ {{0x00B0, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_7},
      // degree sign, *, *
      /* 151 */ {{0x00B0, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_2},
      // superscript two, *, *
      /* 152 */ {{0x00B2, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_7},
      // micro sign, *, *
      /* 153 */ {{0x00B5, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // masculine ordinal indicator, *, *
      /* 154 */ {{0x00BA, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_OEM_5},
      // masculine ordinal indicator, *, *
      /* 155 */ {{0x00BA, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // right-pointing double angle quotation mark, *, *
      /* 156 */ {{0x00BB, 0x2460, 0x2461, DomCode::NONE}, VKEY_9},
      // vulgar fraction one half, *, *
      /* 157 */ {{0x00BD, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // inverted question mark, *, *
      /* 158 */ {{0x00BF, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // sharp s, *, *
      /* 159 */ {{0x00DF, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_4},
      // a grave, degree sign, *
      /* 160 */ {{0x00E0, 0x00B0, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // a grave, a diaeresis, *
      /* 161 */ {{0x00E0, 0x00E4, 0x2461, DomCode::QUOTE}, VKEY_OEM_5},
      // a grave, *, *
      /* 162 */ {{0x00E0, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // a grave, *, *
      /* 163 */ {{0x00E0, 0x2460, 0x2461, DomCode::DIGIT0}, VKEY_0},
      // a acute, *, *
      /* 164 */ {{0x00E1, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // a acute, *, *
      /* 165 */ {{0x00E1, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // a circumflex, *, *
      /* 166 */ {{0x00E2, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // a circumflex, *, *
      /* 167 */ {{0x00E2, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_2},
      // a diaeresis, A diaeresis, unmapped
      /* 168 */ {{0x00E4, 0x00C4, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // a diaeresis, A diaeresis, r caron
      /* 169 */ {{0x00E4, 0x00C4, 0x0159, DomCode::QUOTE}, VKEY_OEM_7},
      // a diaeresis, A diaeresis, S acute
      /* 170 */ {{0x00E4, 0x00C4, 0x015A, DomCode::QUOTE}, VKEY_OEM_7},
      // a diaeresis, a grave, *
      /* 171 */ {{0x00E4, 0x00E0, 0x2461, DomCode::QUOTE}, VKEY_OEM_5},
      // a diaeresis, *, *
      /* 172 */ {{0x00E4, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // a ring above, *, *
      /* 173 */ {{0x00E5, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // ae, *, *
      /* 174 */ {{0x00E6, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // ae, *, *
      /* 175 */ {{0x00E6, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_3},
      // c cedilla, C cedilla, unmapped
      /* 176 */ {{0x00E7, 0x00C7, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // c cedilla, C cedilla, Thorn
      /* 177 */ {{0x00E7, 0x00C7, 0x00DE, DomCode::SEMICOLON}, VKEY_OEM_3},
      // c cedilla, *, *
      /* 178 */ {{0x00E7, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_2},
      // c cedilla, *, *
      /* 179 */ {{0x00E7, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // c cedilla, *, *
      /* 180 */ {{0x00E7, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // c cedilla, *, *
      /* 181 */ {{0x00E7, 0x2460, 0x2461, DomCode::COMMA}, VKEY_OEM_COMMA},
      // c cedilla, *, *
      /* 182 */ {{0x00E7, 0x2460, 0x2461, DomCode::DIGIT9}, VKEY_9},
      // c cedilla, *, *
      /* 183 */ {{0x00E7, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // e grave, *, *
      /* 184 */ {{0x00E8, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_1},
      // e grave, *, *
      /* 185 */ {{0x00E8, 0x2460, 0x2461, DomCode::DIGIT7}, VKEY_7},
      // e grave, *, *
      /* 186 */ {{0x00E8, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_3},
      // e acute, E acute, *
      /* 187 */ {{0x00E9, 0x00C9, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // e acute, o diaeresis, *
      /* 188 */ {{0x00E9, 0x00F6, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_7},
      // e acute, *, *
      /* 189 */ {{0x00E9, 0x2460, 0x2461, DomCode::DIGIT0}, VKEY_0},
      // e acute, *, *
      /* 190 */ {{0x00E9, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_2},
      // e acute, *, *
      /* 191 */ {{0x00E9, 0x2460, 0x2461, DomCode::SLASH}, VKEY_OEM_2},
      // e circumflex, *, *
      /* 192 */ {{0x00EA, 0x2460, 0x2461, DomCode::NONE}, VKEY_3},
      // e diaeresis, *, *
      /* 193 */ {{0x00EB, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_1},
      // i grave, *, *
      /* 194 */ {{0x00EC, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // i acute, *, *
      /* 195 */ {{0x00ED, 0x2460, 0x2461, DomCode::BACKQUOTE}, VKEY_0},
      // i acute, *, *
      /* 196 */ {{0x00ED, 0x2460, 0x2461, DomCode::DIGIT9}, VKEY_9},
      // i circumflex, *, *
      /* 197 */ {{0x00EE, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // eth, *, *
      /* 198 */ {{0x00F0, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_6},
      // eth, *, *
      /* 199 */ {{0x00F0, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_1},
      // n tilde, *, *
      /* 200 */ {{0x00F1, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_3},
      // o grave, *, *
      /* 201 */ {{0x00F2, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_3},
      // o acute, *, *
      /* 202 */ {{0x00F3, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // o acute, *, *
      /* 203 */ {{0x00F3, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_PLUS},
      // o circumflex, *, *
      /* 204 */ {{0x00F4, 0x2460, 0x2461, DomCode::DIGIT4}, VKEY_4},
      // o circumflex, *, *
      /* 205 */ {{0x00F4, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // o tilde, *, *
      /* 206 */ {{0x00F5, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_4},
      // o diaeresis, O diaeresis, unmapped
      /* 207 */ {{0x00F6, 0x00D6, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_3},
      // o diaeresis, O diaeresis, T cedilla
      /* 208 */ {{0x00F6, 0x00D6, 0x0162, DomCode::SEMICOLON}, VKEY_OEM_3},
      // o diaeresis, e acute, *
      /* 209 */ {{0x00F6, 0x00E9, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_7},
      // o diaeresis, *, *
      /* 210 */ {{0x00F6, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // o diaeresis, *, *
      /* 211 */ {{0x00F6, 0x2460, 0x2461, DomCode::DIGIT0}, VKEY_OEM_3},
      // o diaeresis, *, *
      /* 212 */ {{0x00F6, 0x2460, 0x2461, DomCode::MINUS}, VKEY_OEM_PLUS},
      // division sign, *, *
      /* 213 */ {{0x00F7, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // o stroke, *, *
      /* 214 */ {{0x00F8, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // o stroke, *, *
      /* 215 */ {{0x00F8, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_3},
      // u grave, *, *
      /* 216 */ {{0x00F9, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_2},
      // u grave, *, *
      /* 217 */ {{0x00F9, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_3},
      // u acute, *, *
      /* 218 */ {{0x00FA, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // u acute, *, *
      /* 219 */ {{0x00FA, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // u diaeresis, U diaeresis, unmapped
      /* 220 */ {{0x00FC, 0x00DC, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_1},
      // u diaeresis, U diaeresis, unmapped
      /* 221 */ {{0x00FC, 0x00DC, 0x2461, DomCode::MINUS}, VKEY_OEM_2},
      // u diaeresis, U diaeresis, L stroke
      /* 222 */ {{0x00FC, 0x00DC, 0x0141, DomCode::BRACKET_LEFT}, VKEY_OEM_3},
      // u diaeresis, e grave, *
      /* 223 */ {{0x00FC, 0x00E8, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_1},
      // u diaeresis, *, *
      /* 224 */ {{0x00FC, 0x2460, 0x2461, DomCode::US_W}, VKEY_W},
      // y acute, *, *
      /* 225 */ {{0x00FD, 0x2460, 0x2461, DomCode::NONE}, VKEY_7},
      // thorn, *, *
      /* 226 */ {{0x00FE, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_MINUS},
      // a macron, *, *
      /* 227 */ {{0x0101, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_8},
      // a breve, *, *
      /* 228 */ {{0x0103, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // a breve, *, *
      /* 229 */ {{0x0103, 0x2460, 0x2461, DomCode::DIGIT1}, VKEY_1},
      // a ogonek, *, *
      /* 230 */ {{0x0105, 0x2460, 0x2461, DomCode::DIGIT1}, VKEY_1},
      // a ogonek, *, *
      /* 231 */ {{0x0105, 0x2460, 0x2461, DomCode::US_Q}, VKEY_Q},
      // a ogonek, *, *
      /* 232 */ {{0x0105, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // c acute, *, *
      /* 233 */ {{0x0107, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_7},
      // c dot above, *, *
      /* 234 */ {{0x010B, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_3},
      // c caron, *, *
      /* 235 */ {{0x010D, 0x2460, 0x2461, DomCode::COMMA}, VKEY_OEM_COMMA},
      // c caron, *, *
      /* 236 */ {{0x010D, 0x2460, 0x2461, DomCode::DIGIT2}, VKEY_2},
      // c caron, *, *
      /* 237 */ {{0x010D, 0x2460, 0x2461, DomCode::DIGIT4}, VKEY_4},
      // c caron, *, *
      /* 238 */ {{0x010D, 0x2460, 0x2461, DomCode::US_P}, VKEY_X},
      // c caron, *, *
      /* 239 */ {{0x010D, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // d stroke, *, *
      /* 240 */ {{0x0111, 0x2460, 0x2461, DomCode::BRACKET_RIGHT}, VKEY_OEM_6},
      // d stroke, *, *
      /* 241 */ {{0x0111, 0x2460, 0x2461, DomCode::DIGIT0}, VKEY_0},
      // e macron, *, *
      /* 242 */ {{0x0113, 0x2460, 0x2461, DomCode::NONE}, VKEY_W},
      // e dot above, *, *
      /* 243 */ {{0x0117, 0x2460, 0x2461, DomCode::DIGIT4}, VKEY_4},
      // e dot above, *, *
      /* 244 */ {{0x0117, 0x2460, 0x2461, DomCode::QUOTE}, VKEY_OEM_7},
      // e ogonek, E ogonek, unmapped
      /* 245 */ {{0x0119, 0x0118, 0x2461, DomCode::SLASH}, VKEY_OEM_MINUS},
      // e ogonek, E ogonek, n
      /* 246 */ {{0x0119, 0x0118, 0x006E, DomCode::SLASH}, VKEY_OEM_2},
      // e ogonek, *, *
      /* 247 */ {{0x0119, 0x2460, 0x2461, DomCode::DIGIT3}, VKEY_3},
      // e caron, *, *
      /* 248 */ {{0x011B, 0x2460, 0x2461, DomCode::NONE}, VKEY_2},
      // g breve, *, *
      /* 249 */ {{0x011F, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // g dot above, *, *
      /* 250 */ {{0x0121, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_4},
      // h stroke, *, *
      /* 251 */ {{0x0127, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // i macron, *, *
      /* 252 */ {{0x012B, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // i ogonek, I ogonek, unmapped
      /* 253 */ {{0x012F, 0x012E, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // i ogonek, *, *
      /* 254 */ {{0x012F, 0x2460, 0x2461, DomCode::DIGIT5}, VKEY_5},
      // dotless i, *, *
      /* 255 */ {{0x0131, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_1},
      // k cedilla, *, *
      /* 256 */ {{0x0137, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // l cedilla, *, *
      /* 257 */ {{0x013C, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_2},
      // l caron, *, *
      /* 258 */ {{0x013E, 0x2460, 0x2461, DomCode::NONE}, VKEY_2},
      // l stroke, *, *
      /* 259 */ {{0x0142, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_2},
      // l stroke, *, *
      /* 260 */ {{0x0142, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // n cedilla, *, *
      /* 261 */ {{0x0146, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_4},
      // n caron, *, *
      /* 262 */ {{0x0148, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // o double acute, *, *
      /* 263 */ {{0x0151, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_4},
      // r caron, *, *
      /* 264 */ {{0x0159, 0x2460, 0x2461, DomCode::NONE}, VKEY_5},
      // s cedilla, *, *
      /* 265 */ {{0x015F, 0x2460, 0x2461, DomCode::PERIOD}, VKEY_OEM_PERIOD},
      // s cedilla, *, *
      /* 266 */ {{0x015F, 0x2460, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_1},
      // s caron, *, *
      /* 267 */ {{0x0161, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // s caron, *, *
      /* 268 */ {{0x0161, 0x2460, 0x2461, DomCode::DIGIT3}, VKEY_3},
      // s caron, *, *
      /* 269 */ {{0x0161, 0x2460, 0x2461, DomCode::DIGIT6}, VKEY_6},
      // s caron, *, *
      /* 270 */ {{0x0161, 0x2460, 0x2461, DomCode::US_A}, VKEY_OEM_1},
      // s caron, *, *
      /* 271 */ {{0x0161, 0x2460, 0x2461, DomCode::US_F}, VKEY_F},
      // s caron, *, *
      /* 272 */ {{0x0161, 0x2460, 0x2461, DomCode::PERIOD}, VKEY_OEM_PERIOD},
      // t cedilla, *, *
      /* 273 */ {{0x0163, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_7},
      // t caron, *, *
      /* 274 */ {{0x0165, 0x2460, 0x2461, DomCode::NONE}, VKEY_5},
      // u macron, *, *
      /* 275 */ {{0x016B, 0x2460, 0x2461, DomCode::DIGIT8}, VKEY_8},
      // u macron, *, *
      /* 276 */ {{0x016B, 0x2460, 0x2461, DomCode::US_Q}, VKEY_Q},
      // u macron, *, *
      /* 277 */ {{0x016B, 0x2460, 0x2461, DomCode::US_X}, VKEY_X},
      // u ring above, *, *
      /* 278 */ {{0x016F, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_1},
      // u double acute, *, *
      /* 279 */ {{0x0171, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_5},
      // u ogonek, U ogonek, unmapped
      /* 280 */ {{0x0173, 0x0172, 0x2461, DomCode::SEMICOLON}, VKEY_OEM_3},
      // u ogonek, U ogonek, T cedilla
      /* 281 */ {{0x0173, 0x0172, 0x0162, DomCode::SEMICOLON}, VKEY_OEM_1},
      // u ogonek, *, *
      /* 282 */ {{0x0173, 0x2460, 0x2461, DomCode::DIGIT7}, VKEY_7},
      // z dot above, *, *
      /* 283 */ {{0x017C, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // z dot above, *, *
      /* 284 */ {{0x017C, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_OEM_4},
      // z caron, *, *
      /* 285 */ {{0x017E, 0x2460, 0x2461, DomCode::BACKSLASH}, VKEY_OEM_5},
      // z caron, *, *
      /* 286 */ {{0x017E, 0x2460, 0x2461, DomCode::BRACKET_LEFT}, VKEY_Y},
      // z caron, *, *
      /* 287 */ {{0x017E, 0x2460, 0x2461, DomCode::DIGIT6}, VKEY_6},
      // z caron, *, *
      /* 288 */ {{0x017E, 0x2460, 0x2461, DomCode::EQUAL}, VKEY_OEM_PLUS},
      // z caron, *, *
      /* 289 */ {{0x017E, 0x2460, 0x2461, DomCode::US_W}, VKEY_W},
      // o horn, *, *
      /* 290 */ {{0x01A1, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // u horn, *, *
      /* 291 */ {{0x01B0, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_4},
      // z stroke, *, *
      /* 292 */ {{0x01B6, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_6},
      // schwa, *, *
      /* 293 */ {{0x0259, 0x2460, 0x2461, DomCode::NONE}, VKEY_OEM_3},

      // Simple alphanumeric cases.
      /* 294 */ {{'a', 'A', '?', DomCode::NONE}, VKEY_A},
      /* 295 */ {{'z', 'Z', '!', DomCode::NONE}, VKEY_Z},
      /* 296 */ {{'9', '(', '+', DomCode::NONE}, VKEY_9},
      /* 297 */ {{'0', ')', '-', DomCode::NONE}, VKEY_0},

  };

  for (size_t i = 0; i < std::size(kVkeyTestCase); ++i) {
    SCOPED_TRACE(i);
    const auto& e = kVkeyTestCase[i];
    layout_engine_->SetEntry(&e.test);

    // Test with predetermined plain character.
    KeyboardCode key_code = layout_engine_->GetKeyboardCode(
        e.test.dom_code, EF_NONE, e.test.plain_character);
    EXPECT_EQ(e.key_code, key_code);

    if (e.test.shift_character) {
      // Test with predetermined shifted character.
      key_code = layout_engine_->GetKeyboardCode(e.test.dom_code, EF_SHIFT_DOWN,
                                                 e.test.shift_character);
      EXPECT_EQ(e.key_code, key_code);
    }

    if (e.test.altgr_character) {
      // Test with predetermined AltGr character.
      key_code = layout_engine_->GetKeyboardCode(e.test.dom_code, EF_ALTGR_DOWN,
                                                 e.test.altgr_character);
      EXPECT_EQ(e.key_code, key_code);
    }

    // Test with unrelated predetermined character.
    key_code =
        layout_engine_->GetKeyboardCode(e.test.dom_code, EF_MOD3_DOWN, 0xFFFFu);
    EXPECT_EQ(e.key_code, key_code);
  }
}

TEST_F(XkbLayoutEngineVkTest, KeyboardCodeForNonPrintable) {
  static const struct {
    VkTestXkbKeyboardLayoutEngine::KeysymEntry test;
    KeyboardCode key_code;
  } kVkeyTestCase[] = {
      {{DomCode::CONTROL_LEFT, EF_NONE, XKB_KEY_Control_L}, VKEY_CONTROL},
      {{DomCode::CONTROL_RIGHT, EF_NONE, XKB_KEY_Control_R}, VKEY_CONTROL},
      {{DomCode::SHIFT_LEFT, EF_NONE, XKB_KEY_Shift_L}, VKEY_SHIFT},
      {{DomCode::SHIFT_RIGHT, EF_NONE, XKB_KEY_Shift_R}, VKEY_SHIFT},
      {{DomCode::META_LEFT, EF_NONE, XKB_KEY_Super_L}, VKEY_LWIN},
      {{DomCode::META_RIGHT, EF_NONE, XKB_KEY_Super_R}, VKEY_LWIN},
      {{DomCode::ALT_LEFT, EF_NONE, XKB_KEY_Alt_L}, VKEY_MENU},
      {{DomCode::ALT_RIGHT, EF_NONE, XKB_KEY_Alt_R}, VKEY_MENU},
      {{DomCode::ALT_RIGHT, EF_NONE, XKB_KEY_ISO_Level3_Shift}, VKEY_ALTGR},
      {{DomCode::DIGIT1, EF_NONE, XKB_KEY_1}, VKEY_1},
      {{DomCode::NUMPAD1, EF_NONE, XKB_KEY_KP_1}, VKEY_1},
      {{DomCode::CAPS_LOCK, EF_NONE, XKB_KEY_Caps_Lock}, VKEY_CAPITAL},
      {{DomCode::ENTER, EF_NONE, XKB_KEY_Return}, VKEY_RETURN},
      {{DomCode::NUMPAD_ENTER, EF_NONE, XKB_KEY_KP_Enter}, VKEY_RETURN},
      {{DomCode::SLEEP, EF_NONE, XKB_KEY_XF86Sleep}, VKEY_SLEEP},
      // Verify that we can translate some Dom codes even if they are not
      // known to XKB.
      {{DomCode::LAUNCH_ASSISTANT, EF_NONE}, VKEY_ASSISTANT},
      {{DomCode::LAUNCH_CONTROL_PANEL, EF_NONE}, VKEY_SETTINGS},
      {{DomCode::PRIVACY_SCREEN_TOGGLE, EF_NONE}, VKEY_PRIVACY_SCREEN_TOGGLE},
      {{DomCode::MICROPHONE_MUTE_TOGGLE, EF_NONE}, VKEY_MICROPHONE_MUTE_TOGGLE},
      {{DomCode::EMOJI_PICKER, EF_NONE}, VKEY_EMOJI_PICKER},
      {{DomCode::DICTATE, EF_NONE}, VKEY_DICTATE},
      {{DomCode::ALL_APPLICATIONS, EF_NONE}, VKEY_ALL_APPLICATIONS},
      {{DomCode::ACCESSIBILITY, EF_NONE}, VKEY_ACCESSIBILITY},
      // Verify the AC Application keys.
      {{DomCode::NEW, EF_NONE}, VKEY_NEW},
      {{DomCode::CLOSE, EF_NONE}, VKEY_CLOSE},
      // Verify that number pad digits produce located VKEY codes.
      {{DomCode::NUMPAD0, EF_NONE, XKB_KEY_KP_0, '0'}, VKEY_NUMPAD0},
      {{DomCode::NUMPAD9, EF_NONE, XKB_KEY_KP_9, '9'}, VKEY_NUMPAD9},
      // Verify AltGr+V & AltGr+W on de(neo) layout.
      {{DomCode::US_W, EF_ALTGR_DOWN, XKB_KEY_BackSpace, 8}, VKEY_BACK},
      {{DomCode::US_V, EF_ALTGR_DOWN, XKB_KEY_Return, 13}, VKEY_RETURN},
#if BUILDFLAG(IS_CHROMEOS)
      // Verify on ChromeOS PRINT maps to VKEY_PRINT not VKEY_SNAPSHOT.
      {{DomCode::PRINT, EF_NONE, XKB_KEY_Print}, VKEY_PRINT},
      // On ChromeOS XKB_KEY_3270_PrintScreen is used for PRINT_SCREEN.
      {{DomCode::PRINT_SCREEN, EF_NONE, XKB_KEY_3270_PrintScreen},
       VKEY_SNAPSHOT},
#else   // !BUILDFLAG(IS_CHROMEOS)
      // On Linux PRINT and PRINT_SCREEN map to VKEY_SNAPSHOT via XKB_KEY_Print
      {{DomCode::PRINT, EF_NONE, XKB_KEY_Print}, VKEY_SNAPSHOT},
      {{DomCode::PRINT_SCREEN, EF_NONE, XKB_KEY_Print}, VKEY_SNAPSHOT},
#endif  // BUILDFLAG(IS_CHROMEOS)
  };
  for (const auto& e : kVkeyTestCase) {
    SCOPED_TRACE(static_cast<int>(e.test.dom_code));
    layout_engine_->SetEntry(&e.test);
    DomKey dom_key = DomKey::NONE;
    KeyboardCode key_code = VKEY_UNKNOWN;
    EXPECT_TRUE(layout_engine_->Lookup(e.test.dom_code, e.test.flags, &dom_key,
                                       &key_code));
    EXPECT_EQ(e.key_code, key_code);
  }
}


TEST_F(XkbLayoutEngineVkTest, XkbRuleNamesForLayoutName) {
  static const VkTestXkbKeyboardLayoutEngine::RuleNames kVkeyTestCase[] = {
      /* 0 */ {"us", "us", ""},
      /* 1 */ {"jp", "jp", ""},
      /* 2 */ {"us(intl)", "us", "intl"},
      /* 3 */ {"us(altgr-intl)", "us", "altgr-intl"},
      /* 4 */ {"us(dvorak)", "us", "dvorak"},
      /* 5 */ {"us(colemak)", "us", "colemak"},
      /* 6 */ {"be", "be", ""},
      /* 7 */ {"fr", "fr", ""},
      /* 8 */ {"ca", "ca", ""},
      /* 9 */ {"ch(fr)", "ch", "fr"},
      /* 10 */ {"ca(multix)", "ca", "multix"},
      /* 11 */ {"de", "de", ""},
      /* 12 */ {"de(neo)", "de", "neo"},
      /* 13 */ {"ch", "ch", ""},
      /* 14 */ {"ru", "ru", ""},
      /* 15 */ {"ru(phonetic)", "ru", "phonetic"},
      /* 16 */ {"br", "br", ""},
      /* 17 */ {"bg", "bg", ""},
      /* 18 */ {"bg(phonetic)", "bg", "phonetic"},
      /* 19 */ {"ca(eng)", "ca", "eng"},
      /* 20 */ {"cz", "cz", ""},
      /* 21 */ {"cz(qwerty)", "cz", "qwerty"},
      /* 22 */ {"ee", "ee", ""},
      /* 23 */ {"es", "es", ""},
      /* 24 */ {"es(cat)", "es", "cat"},
      /* 25 */ {"dk", "dk", ""},
      /* 26 */ {"gr", "gr", ""},
      /* 27 */ {"il", "il", ""},
      /* 28 */ {"latam", "latam", ""},
      /* 29 */ {"lt", "lt", ""},
      /* 30 */ {"lv(apostrophe)", "lv", "apostrophe"},
      /* 31 */ {"hr", "hr", ""},
      /* 32 */ {"gb(extd)", "gb", "extd"},
      /* 33 */ {"gb(dvorak)", "gb", "dvorak"},
      /* 34 */ {"fi", "fi", ""},
      /* 35 */ {"hu", "hu", ""},
      /* 36 */ {"it", "it", ""},
      /* 37 */ {"is", "is", ""},
      /* 38 */ {"no", "no", ""},
      /* 39 */ {"pl", "pl", ""},
      /* 40 */ {"pt", "pt", ""},
      /* 41 */ {"ro", "ro", ""},
      /* 42 */ {"se", "se", ""},
      /* 43 */ {"sk", "sk", ""},
      /* 44 */ {"si", "si", ""},
      /* 45 */ {"rs", "rs", ""},
      /* 46 */ {"tr", "tr", ""},
      /* 47 */ {"ua", "ua", ""},
      /* 48 */ {"by", "by", ""},
      /* 49 */ {"am", "am", ""},
      /* 50 */ {"ge", "ge", ""},
      /* 51 */ {"mn", "mn", ""},
      /* 52 */ {"ie", "ie", ""}};
  for (size_t i = 0; i < std::size(kVkeyTestCase); ++i) {
    SCOPED_TRACE(i);
    const VkTestXkbKeyboardLayoutEngine::RuleNames* e = &kVkeyTestCase[i];
    std::string layout_id;
    std::string layout_variant;
    XkbKeyboardLayoutEngine::ParseLayoutName(e->layout_name, &layout_id,
                                             &layout_variant);
    EXPECT_EQ(layout_id, e->layout);
    EXPECT_EQ(layout_variant, e->variant);
  }
}

TEST_F(XkbLayoutEngineVkTest, GetDomCodeByKeysym) {
  // Set up US keyboard layout.
  {
    std::unique_ptr<xkb_context, ui::XkbContextDeleter> xkb_context(
        xkb_context_new(XKB_CONTEXT_NO_FLAGS));
    xkb_rule_names names = {
        .rules = nullptr,
        .model = "pc101",
        .layout = "us",
        .variant = "",
        .options = "",
    };
    std::unique_ptr<xkb_keymap, ui::XkbKeymapDeleter> xkb_keymap(
        xkb_keymap_new_from_names(xkb_context.get(), &names,
                                  XKB_KEYMAP_COMPILE_NO_FLAGS));
    std::unique_ptr<char, base::FreeDeleter> layout(
        xkb_keymap_get_as_string(xkb_keymap.get(), XKB_KEYMAP_FORMAT_TEXT_V1));
    layout_engine_->SetCurrentLayoutFromBuffer(layout.get(),
                                               std::strlen(layout.get()));
  }

  constexpr int32_t kNullopt = -1;
  constexpr int32_t kShiftMask = 1;
  constexpr int32_t kCapsLockMask = 2;
  constexpr int32_t kNumLockMask = 4;
  constexpr struct {
    uint32_t keysym;
    int32_t modifiers;
    DomCode expected_dom_code;
  } kTestCases[] = {
    {65307, 0, ui::DomCode::ESCAPE},
    {65288, 0, ui::DomCode::BACKSPACE},
    {65293, 0, ui::DomCode::ENTER},
    {65289, 0, ui::DomCode::TAB},
    {65056, kShiftMask, ui::DomCode::TAB},

    // Test conflict keysym case. We use '<' as a testing example.
    // On pc101 layout, intl_backslash is expected without SHIFT modifier.
    {60, 0, ui::DomCode::INTL_BACKSLASH},
    // And, if SHIFT is pressed, comma key is expected.
    {60, kShiftMask, ui::DomCode::COMMA},

    // Test for space key. The keysym mapping has only one keycode entry.
    // It expects all modifiers are ignored. Used SHIFT as testing example.
    {32, 0, ui::DomCode::SPACE},
    {32, kShiftMask, ui::DomCode::SPACE},

    // For checking regression case we hit in past.
    // CapsLock + A.
    {65, kNullopt, ui::DomCode::US_A},
    {65, kCapsLockMask, ui::DomCode::US_A},

    // NumLock + Numpad1. NumLock
    {65457, kNullopt, ui::DomCode::NUMPAD1},
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // On ChromeOS, NumLock should be interpreted as it is always set.
    {65457, 0, ui::DomCode::NUMPAD1},
#endif
    {65457, kNumLockMask, ui::DomCode::NUMPAD1},
  };

  for (const auto& test_case : kTestCases) {
    std::optional<std::vector<std::string_view>> modifiers;
    if (test_case.modifiers != kNullopt) {
      std::vector<std::string_view> modifiers_content;
      if (test_case.modifiers & kShiftMask)
        modifiers_content.push_back(XKB_MOD_NAME_SHIFT);
      if (test_case.modifiers & kCapsLockMask)
        modifiers_content.push_back(XKB_MOD_NAME_CAPS);
      if (test_case.modifiers & kNumLockMask)
        modifiers_content.push_back(XKB_MOD_NAME_NUM);
      modifiers = std::move(modifiers_content);
    }

    EXPECT_EQ(test_case.expected_dom_code,
              layout_engine_->GetDomCodeByKeysym(test_case.keysym, modifiers))
        << "input: " << test_case.keysym << ", " << test_case.modifiers;
  }
}

}  // namespace ui
