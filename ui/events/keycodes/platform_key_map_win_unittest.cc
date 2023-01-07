// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/platform_key_map_win.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/dom_us_layout_data.h"
#include "ui/events/test/keyboard_layout.h"

namespace ui {

namespace {

struct TestKey {
  KeyboardCode key_code;
  const char* normal;
  const char* shift;
  const char* capslock;
  const char* altgr;
  const char* shift_capslock;
  const char* shift_altgr;
  const char* altgr_capslock;
};

struct DomKeyAndFlags {
  DomKey key;
  int flags;
};

}  // anonymous namespace

class PlatformKeyMapTest : public testing::Test {
 public:
  PlatformKeyMapTest() {}

  PlatformKeyMapTest(const PlatformKeyMapTest&) = delete;
  PlatformKeyMapTest& operator=(const PlatformKeyMapTest&) = delete;

  ~PlatformKeyMapTest() override {}

  void CheckKeyboardCodeToKeyString(const char* label,
                                    const PlatformKeyMap& keymap,
                                    const TestKey& test_case) {
    KeyboardCode key_code = test_case.key_code;
    EXPECT_STREQ(test_case.normal,
                 KeycodeConverter::DomKeyToKeyString(
                     DomKeyFromKeyboardCodeImpl(keymap, key_code, EF_NONE))
                     .c_str())
        << label;
    EXPECT_STREQ(test_case.shift, KeycodeConverter::DomKeyToKeyString(
                                      DomKeyFromKeyboardCodeImpl(
                                          keymap, key_code, EF_SHIFT_DOWN))
                                      .c_str())
        << label;
    EXPECT_STREQ(test_case.capslock, KeycodeConverter::DomKeyToKeyString(
                                         DomKeyFromKeyboardCodeImpl(
                                             keymap, key_code, EF_CAPS_LOCK_ON))
                                         .c_str())
        << label;
    EXPECT_STREQ(test_case.altgr, KeycodeConverter::DomKeyToKeyString(
                                      DomKeyFromKeyboardCodeImpl(
                                          keymap, key_code, EF_ALTGR_DOWN))
                                      .c_str())
        << label;
    EXPECT_STREQ(test_case.shift_capslock,
                 KeycodeConverter::DomKeyToKeyString(
                     DomKeyFromKeyboardCodeImpl(
                         keymap, key_code, EF_SHIFT_DOWN | EF_CAPS_LOCK_ON))
                     .c_str())
        << label;
    EXPECT_STREQ(test_case.shift_altgr,
                 KeycodeConverter::DomKeyToKeyString(
                     DomKeyFromKeyboardCodeImpl(keymap, key_code,
                                                EF_SHIFT_DOWN | EF_ALTGR_DOWN))
                     .c_str())
        << label;
    EXPECT_STREQ(test_case.altgr_capslock,
                 KeycodeConverter::DomKeyToKeyString(
                     DomKeyFromKeyboardCodeImpl(
                         keymap, key_code, EF_ALTGR_DOWN | EF_CAPS_LOCK_ON))
                     .c_str())
        << label;
  }

  // Need this helper function to access private methods of |PlatformKeyMap|.
  DomKey DomKeyFromKeyboardCodeImpl(const PlatformKeyMap& keymap,
                                    KeyboardCode key_code,
                                    int flags) {
    return keymap.DomKeyFromKeyboardCodeImpl(key_code, &flags);
  }

  // Returns the DomKey and |flags| in a struct, for use in tests verifying
  // that the API correctly modifies the |flags| in/out parameter.
  DomKeyAndFlags DomKeyAndFlagsFromKeyboardCode(const PlatformKeyMap& keymap,
                                                KeyboardCode key_code,
                                                int flags) {
    DomKeyAndFlags result = {DomKey(), flags};
    result.key = keymap.DomKeyFromKeyboardCodeImpl(key_code, &result.flags);
    return result;
  }
};

TEST_F(PlatformKeyMapTest, USLayout) {
  PlatformKeyMap keymap(GetPlatformKeyboardLayout(KEYBOARD_LAYOUT_ENGLISH_US));

  const TestKey kUSLayoutTestCases[] = {
      //       n    s    c    a    sc   sa   ac
      {VKEY_0, "0", ")", "0", "0", ")", ")", "0"},
      {VKEY_1, "1", "!", "1", "1", "!", "!", "1"},
      {VKEY_2, "2", "@", "2", "2", "@", "@", "2"},
      {VKEY_3, "3", "#", "3", "3", "#", "#", "3"},
      {VKEY_4, "4", "$", "4", "4", "$", "$", "4"},
      {VKEY_5, "5", "%", "5", "5", "%", "%", "5"},
      {VKEY_6, "6", "^", "6", "6", "^", "^", "6"},
      {VKEY_7, "7", "&", "7", "7", "&", "&", "7"},
      {VKEY_8, "8", "*", "8", "8", "*", "*", "8"},
      {VKEY_9, "9", "(", "9", "9", "(", "(", "9"},
      {VKEY_A, "a", "A", "A", "a", "a", "A", "A"},
      {VKEY_B, "b", "B", "B", "b", "b", "B", "B"},
      {VKEY_C, "c", "C", "C", "c", "c", "C", "C"},
      {VKEY_D, "d", "D", "D", "d", "d", "D", "D"},
      {VKEY_E, "e", "E", "E", "e", "e", "E", "E"},
      {VKEY_F, "f", "F", "F", "f", "f", "F", "F"},
      {VKEY_G, "g", "G", "G", "g", "g", "G", "G"},
      {VKEY_H, "h", "H", "H", "h", "h", "H", "H"},
      {VKEY_I, "i", "I", "I", "i", "i", "I", "I"},
      {VKEY_J, "j", "J", "J", "j", "j", "J", "J"},
      {VKEY_K, "k", "K", "K", "k", "k", "K", "K"},
      {VKEY_L, "l", "L", "L", "l", "l", "L", "L"},
      {VKEY_M, "m", "M", "M", "m", "m", "M", "M"},
      {VKEY_N, "n", "N", "N", "n", "n", "N", "N"},
      {VKEY_O, "o", "O", "O", "o", "o", "O", "O"},
      {VKEY_P, "p", "P", "P", "p", "p", "P", "P"},
      {VKEY_Q, "q", "Q", "Q", "q", "q", "Q", "Q"},
      {VKEY_R, "r", "R", "R", "r", "r", "R", "R"},
      {VKEY_S, "s", "S", "S", "s", "s", "S", "S"},
      {VKEY_T, "t", "T", "T", "t", "t", "T", "T"},
      {VKEY_U, "u", "U", "U", "u", "u", "U", "U"},
      {VKEY_V, "v", "V", "V", "v", "v", "V", "V"},
      {VKEY_W, "w", "W", "W", "w", "w", "W", "W"},
      {VKEY_X, "x", "X", "X", "x", "x", "X", "X"},
      {VKEY_Y, "y", "Y", "Y", "y", "y", "Y", "Y"},
      {VKEY_Z, "z", "Z", "Z", "z", "z", "Z", "Z"},
  };

  for (const auto& test_case : kUSLayoutTestCases) {
    CheckKeyboardCodeToKeyString("USLayout", keymap, test_case);
  }
}

TEST_F(PlatformKeyMapTest, FRLayout) {
  PlatformKeyMap keymap(GetPlatformKeyboardLayout(KEYBOARD_LAYOUT_FRENCH));

  const TestKey kFRLayoutTestCases[] = {
      //       n     s    c    a       sc    sa   ac
      {VKEY_0, "à",  "0", "0", "@",    "à",  "0", "@"},
      {VKEY_1, "&",  "1", "1", "&",    "&",  "1", "1"},
      {VKEY_2, "é",  "2", "2", "Dead", "é",  "2", "Dead"},
      {VKEY_3, "\"", "3", "3", "#",    "\"", "3", "#"},
      {VKEY_4, "\'", "4", "4", "{",    "\'", "4", "{"},
      {VKEY_5, "(",  "5", "5", "[",    "(",  "5", "["},
      {VKEY_6, "-",  "6", "6", "|",    "-",  "6", "|"},
      {VKEY_7, "è",  "7", "7", "Dead", "è",  "7", "Dead"},
      {VKEY_8, "_",  "8", "8", "\\",   "_",  "8", "\\"},
      {VKEY_9, "ç",  "9", "9", "^",    "ç",  "9", "^"},
      {VKEY_A, "a",  "A", "A", "a",    "a",  "A", "A"},
      {VKEY_B, "b",  "B", "B", "b",    "b",  "B", "B"},
      {VKEY_C, "c",  "C", "C", "c",    "c",  "C", "C"},
      {VKEY_D, "d",  "D", "D", "d",    "d",  "D", "D"},
      {VKEY_E, "e",  "E", "E", "€",    "e",  "E", "€"},
      {VKEY_F, "f",  "F", "F", "f",    "f",  "F", "F"},
      {VKEY_G, "g",  "G", "G", "g",    "g",  "G", "G"},
      {VKEY_H, "h",  "H", "H", "h",    "h",  "H", "H"},
      {VKEY_I, "i",  "I", "I", "i",    "i",  "I", "I"},
      {VKEY_J, "j",  "J", "J", "j",    "j",  "J", "J"},
      {VKEY_K, "k",  "K", "K", "k",    "k",  "K", "K"},
      {VKEY_L, "l",  "L", "L", "l",    "l",  "L", "L"},
      {VKEY_M, "m",  "M", "M", "m",    "m",  "M", "M"},
      {VKEY_N, "n",  "N", "N", "n",    "n",  "N", "N"},
      {VKEY_O, "o",  "O", "O", "o",    "o",  "O", "O"},
      {VKEY_P, "p",  "P", "P", "p",    "p",  "P", "P"},
      {VKEY_Q, "q",  "Q", "Q", "q",    "q",  "Q", "Q"},
      {VKEY_R, "r",  "R", "R", "r",    "r",  "R", "R"},
      {VKEY_S, "s",  "S", "S", "s",    "s",  "S", "S"},
      {VKEY_T, "t",  "T", "T", "t",    "t",  "T", "T"},
      {VKEY_U, "u",  "U", "U", "u",    "u",  "U", "U"},
      {VKEY_V, "v",  "V", "V", "v",    "v",  "V", "V"},
      {VKEY_W, "w",  "W", "W", "w",    "w",  "W", "W"},
      {VKEY_X, "x",  "X", "X", "x",    "x",  "X", "X"},
      {VKEY_Y, "y",  "Y", "Y", "y",    "y",  "Y", "Y"},
      {VKEY_Z, "z",  "Z", "Z", "z",    "z",  "Z", "Z"},
  };

  for (const auto& test_case : kFRLayoutTestCases) {
    CheckKeyboardCodeToKeyString("FRLayout", keymap, test_case);
  }
}

TEST_F(PlatformKeyMapTest, NumPad) {
  PlatformKeyMap keymap(GetPlatformKeyboardLayout(KEYBOARD_LAYOUT_ENGLISH_US));

  const struct TestCase {
    KeyboardCode key_code;
    DomKey key;
  } kNumPadTestCases[] = {
      {VKEY_NUMPAD0, DomKey::FromCharacter('0')},
      {VKEY_NUMPAD1, DomKey::FromCharacter('1')},
      {VKEY_NUMPAD2, DomKey::FromCharacter('2')},
      {VKEY_NUMPAD3, DomKey::FromCharacter('3')},
      {VKEY_NUMPAD4, DomKey::FromCharacter('4')},
      {VKEY_NUMPAD5, DomKey::FromCharacter('5')},
      {VKEY_NUMPAD6, DomKey::FromCharacter('6')},
      {VKEY_NUMPAD7, DomKey::FromCharacter('7')},
      {VKEY_NUMPAD8, DomKey::FromCharacter('8')},
      {VKEY_NUMPAD9, DomKey::FromCharacter('9')},
      {VKEY_CLEAR, DomKey::CLEAR},
      {VKEY_PRIOR, DomKey::PAGE_UP},
      {VKEY_NEXT, DomKey::PAGE_DOWN},
      {VKEY_END, DomKey::END},
      {VKEY_HOME, DomKey::HOME},
      {VKEY_LEFT, DomKey::ARROW_LEFT},
      {VKEY_UP, DomKey::ARROW_UP},
      {VKEY_RIGHT, DomKey::ARROW_RIGHT},
      {VKEY_DOWN, DomKey::ARROW_DOWN},
      {VKEY_INSERT, DomKey::INSERT},
      {VKEY_DELETE, DomKey::DEL},
  };

  for (const auto& test_case : kNumPadTestCases) {
    KeyboardCode key_code = test_case.key_code;

    EXPECT_EQ(test_case.key,
              DomKeyFromKeyboardCodeImpl(keymap, key_code, EF_NONE))
        << key_code;
    EXPECT_EQ(test_case.key,
              DomKeyFromKeyboardCodeImpl(keymap, key_code, EF_ALTGR_DOWN))
        << key_code;
    EXPECT_EQ(test_case.key,
              DomKeyFromKeyboardCodeImpl(keymap, key_code, EF_CONTROL_DOWN))
        << key_code;
    EXPECT_EQ(test_case.key,
              DomKeyFromKeyboardCodeImpl(keymap, key_code,
                                         EF_ALTGR_DOWN | EF_CONTROL_DOWN))
        << key_code;
  }
}

TEST_F(PlatformKeyMapTest, NonPrintableKey) {
  HKL layout = GetPlatformKeyboardLayout(KEYBOARD_LAYOUT_ENGLISH_US);
  PlatformKeyMap keymap(layout);

  for (const auto& test_case : kNonPrintableCodeMap) {
    // Not available on |LAYOUT_US|.
    if (test_case.dom_code == DomCode::PAUSE ||
        test_case.dom_code == DomCode::LANG2 ||
        test_case.dom_code == DomCode::NON_CONVERT)
      continue;

    int scan_code =
        ui::KeycodeConverter::DomCodeToNativeKeycode(test_case.dom_code);
    // TODO(input-dev): Some |scan_code| should map to different |key_code|
    // based on modifiers.
    KeyboardCode key_code = static_cast<KeyboardCode>(
        ::MapVirtualKeyEx(scan_code, MAPVK_VSC_TO_VK, layout));

    if (key_code == VKEY_UNKNOWN)
      continue;

    EXPECT_EQ(test_case.dom_key,
              DomKeyFromKeyboardCodeImpl(keymap, key_code, EF_NONE))
        << key_code << ", " << scan_code;
    EXPECT_EQ(test_case.dom_key,
              DomKeyFromKeyboardCodeImpl(keymap, key_code, EF_ALTGR_DOWN))
        << key_code << ", " << scan_code;
    EXPECT_EQ(test_case.dom_key,
              DomKeyFromKeyboardCodeImpl(keymap, key_code, EF_CONTROL_DOWN))
        << key_code << ", " << scan_code;
    EXPECT_EQ(test_case.dom_key,
              DomKeyFromKeyboardCodeImpl(keymap, key_code,
                                         EF_ALTGR_DOWN | EF_CONTROL_DOWN))
        << key_code << ", " << scan_code;
  }
}

TEST_F(PlatformKeyMapTest, KoreanSpecificKeys) {
  const struct TestCase {
    KeyboardCode key_code;
    DomKey kr_key;
    DomKey us_key;
  } kKoreanTestCases[] = {
      {VKEY_HANGUL, DomKey::HANGUL_MODE, DomKey::UNIDENTIFIED},
      {VKEY_HANJA, DomKey::HANJA_MODE, DomKey::UNIDENTIFIED},
  };

  PlatformKeyMap us_keymap(
      GetPlatformKeyboardLayout(KEYBOARD_LAYOUT_ENGLISH_US));
  PlatformKeyMap kr_keymap(GetPlatformKeyboardLayout(KEYBOARD_LAYOUT_KOREAN));
  for (const auto& test_case : kKoreanTestCases) {
    EXPECT_EQ(test_case.us_key, DomKeyFromKeyboardCodeImpl(
                                    us_keymap, test_case.key_code, EF_NONE))
        << test_case.key_code;
    EXPECT_EQ(test_case.kr_key, DomKeyFromKeyboardCodeImpl(
                                    kr_keymap, test_case.key_code, EF_NONE))
        << test_case.key_code;
  }
}

TEST_F(PlatformKeyMapTest, JapaneseSpecificKeys) {
  const struct TestCase {
    KeyboardCode key_code;
    DomKey jp_key;
    DomKey us_key;
  } kJapaneseTestCases[] = {
      {VKEY_KANA, DomKey::KANA_MODE, DomKey::UNIDENTIFIED},
      {VKEY_KANJI, DomKey::KANJI_MODE, DomKey::UNIDENTIFIED},
      {VKEY_OEM_ATTN, DomKey::ALPHANUMERIC, DomKey::UNIDENTIFIED},
      {VKEY_OEM_FINISH, DomKey::KATAKANA, DomKey::UNIDENTIFIED},
      {VKEY_OEM_COPY, DomKey::HIRAGANA, DomKey::UNIDENTIFIED},
      {VKEY_DBE_SBCSCHAR, DomKey::HANKAKU, DomKey::UNIDENTIFIED},
      {VKEY_DBE_DBCSCHAR, DomKey::ZENKAKU, DomKey::UNIDENTIFIED},
      {VKEY_OEM_BACKTAB, DomKey::ROMAJI, DomKey::UNIDENTIFIED},
      {VKEY_ATTN, DomKey::KANA_MODE, DomKey::ATTN},
  };

  PlatformKeyMap us_keymap(
      GetPlatformKeyboardLayout(KEYBOARD_LAYOUT_ENGLISH_US));
  PlatformKeyMap jp_keymap(GetPlatformKeyboardLayout(KEYBOARD_LAYOUT_JAPANESE));
  for (const auto& test_case : kJapaneseTestCases) {
    EXPECT_EQ(test_case.us_key, DomKeyFromKeyboardCodeImpl(
                                    us_keymap, test_case.key_code, EF_NONE))
        << test_case.key_code;
    EXPECT_EQ(test_case.jp_key, DomKeyFromKeyboardCodeImpl(
                                    jp_keymap, test_case.key_code, EF_NONE))
        << test_case.key_code;
  }
}

TEST_F(PlatformKeyMapTest, AltGraphDomKey) {
  PlatformKeyMap us_keymap(
      GetPlatformKeyboardLayout(KEYBOARD_LAYOUT_ENGLISH_US));
  EXPECT_EQ(DomKey::ALT,
            DomKeyFromKeyboardCodeImpl(us_keymap, VKEY_MENU, EF_ALTGR_DOWN));
  EXPECT_EQ(DomKey::ALT,
            DomKeyFromKeyboardCodeImpl(us_keymap, VKEY_MENU,
                                       EF_ALTGR_DOWN | EF_IS_EXTENDED_KEY));

  PlatformKeyMap fr_keymap(GetPlatformKeyboardLayout(KEYBOARD_LAYOUT_FRENCH));
  EXPECT_EQ(DomKey::ALT,
            DomKeyFromKeyboardCodeImpl(fr_keymap, VKEY_MENU, EF_ALTGR_DOWN));
  EXPECT_EQ(DomKey::ALT_GRAPH,
            DomKeyFromKeyboardCodeImpl(fr_keymap, VKEY_MENU,
                                       EF_ALTGR_DOWN | EF_IS_EXTENDED_KEY));
}

namespace {

const struct AltGraphModifierTestCase {
  // Test-case Virtual Keycode and modifier flags.
  KeyboardCode key_code;
  int flags;

  // Whether or not this case generates an AltGraph-shifted key under FR-fr
  // layout.
  bool expect_alt_graph;
} kAltGraphModifierTestCases[] = {
    {VKEY_C, EF_NONE, false},
    {VKEY_C, EF_ALTGR_DOWN, false},
    {VKEY_C, EF_CONTROL_DOWN | EF_ALT_DOWN, false},
    {VKEY_C, EF_CONTROL_DOWN | EF_ALT_DOWN | EF_ALTGR_DOWN, false},
    {VKEY_E, EF_NONE, false},
    {VKEY_E, EF_ALTGR_DOWN, true},
    {VKEY_E, EF_CONTROL_DOWN | EF_ALT_DOWN, true},
    {VKEY_E, EF_CONTROL_DOWN | EF_ALT_DOWN | EF_ALTGR_DOWN, true},
};

class AltGraphModifierTest
    : public PlatformKeyMapTest,
      public testing::WithParamInterface<KeyboardLayout> {
 public:
  AltGraphModifierTest() : keymap_(GetPlatformKeyboardLayout(GetParam())) {}

 protected:
  PlatformKeyMap keymap_;
};

TEST_P(AltGraphModifierTest, AltGraphModifierBehaviour) {
  // If the key generates a character under AltGraph then |result| should
  // report AltGraph, but not Control or Alt.
  for (const auto& test_case : kAltGraphModifierTestCases) {
    DomKeyAndFlags result = DomKeyAndFlagsFromKeyboardCode(
        keymap_, test_case.key_code, test_case.flags);
    if (GetParam() == KEYBOARD_LAYOUT_FRENCH && test_case.expect_alt_graph) {
      EXPECT_EQ(EF_ALTGR_DOWN, result.flags)
          << " for key_code=" << test_case.key_code
          << " flags=" << test_case.flags;
    } else {
      EXPECT_EQ(test_case.flags, result.flags)
          << " for key_code=" << test_case.key_code
          << " flags=" << test_case.flags;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(VerifyAltGraph,
                         AltGraphModifierTest,
                         ::testing::Values(KEYBOARD_LAYOUT_ENGLISH_US,
                                           KEYBOARD_LAYOUT_FRENCH));

}  // namespace

}  // namespace ui
