// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/events/keycodes/keyboard_code_conversion_ios.h"

#import <UIKit/UIKit.h>

#include <algorithm>
#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/third_party/icu/icu_utf.h"
#include "build/build_config.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

#if !BUILDFLAG(IS_IOS_TVOS)
#import <BrowserEngineKit/BrowserEngineKit.h>
#endif

namespace ui {

namespace {

// See https://w3c.github.io/uievents-key/#selecting-key-attribute-values.
// constexpr int kGlyphModifiers = UIKeyModifierShift |
//                                UIKeyModifierAlphaShift |
//                                UIKeyModifierAlternate;

// Returns whether the given key is a valid DOM key character.
// See https://w3c.github.io/uievents-key/#key-string.
bool IsDomKeyUnicodeCharacter(char32_t c) {
  return base::IsValidCodepoint(c) && !base::IsUnicodeControl(c);
}

DomKey DomKeyFromKeyCode(unsigned short key_code) {
  constexpr auto kMap = base::MakeFixedFlatMap<unsigned short, DomKey>({
      {VKEY_RETURN, DomKey::ENTER},
      {VKEY_TAB, DomKey::TAB},
      {VKEY_BACK, DomKey::BACKSPACE},
      {VKEY_ESCAPE, DomKey::ESCAPE},
      {VKEY_LWIN, DomKey::META},
      {VKEY_RWIN, DomKey::META},
      {VKEY_LSHIFT, DomKey::SHIFT},
      {VKEY_RSHIFT, DomKey::SHIFT},
      {VKEY_CAPITAL, DomKey::CAPS_LOCK},
      {VKEY_LMENU, DomKey::ALT},
      {VKEY_RMENU, DomKey::ALT},
      {VKEY_LCONTROL, DomKey::CONTROL},
      {VKEY_RCONTROL, DomKey::CONTROL},
      {VKEY_UNKNOWN, DomKey::FN},
      {VKEY_VOLUME_UP, DomKey::AUDIO_VOLUME_UP},
      {VKEY_VOLUME_DOWN, DomKey::AUDIO_VOLUME_DOWN},
      {VKEY_VOLUME_MUTE, DomKey::AUDIO_VOLUME_MUTE},
      {VKEY_F1, DomKey::F1},
      {VKEY_F2, DomKey::F2},
      {VKEY_F3, DomKey::F3},
      {VKEY_F4, DomKey::F4},
      {VKEY_F5, DomKey::F5},
      {VKEY_F6, DomKey::F6},
      {VKEY_F7, DomKey::F7},
      {VKEY_F8, DomKey::F8},
      {VKEY_F9, DomKey::F9},
      {VKEY_F10, DomKey::F10},
      {VKEY_F11, DomKey::F11},
      {VKEY_F12, DomKey::F12},
      {VKEY_F13, DomKey::F13},
      {VKEY_F14, DomKey::F14},
      {VKEY_F15, DomKey::F15},
      {VKEY_F16, DomKey::F16},
      {VKEY_F17, DomKey::F17},
      {VKEY_F18, DomKey::F18},
      {VKEY_F19, DomKey::F19},
      {VKEY_F20, DomKey::F20},
      {VKEY_HELP, DomKey::HELP},
      {VKEY_HOME, DomKey::HOME},
      {VKEY_PRIOR, DomKey::PAGE_UP},
      {VKEY_DELETE, DomKey::DEL},
      {VKEY_END, DomKey::END},
      {VKEY_NEXT, DomKey::PAGE_DOWN},
      {VKEY_LEFT, DomKey::ARROW_LEFT},
      {VKEY_RIGHT, DomKey::ARROW_RIGHT},
      {VKEY_DOWN, DomKey::ARROW_DOWN},
      {VKEY_UP, DomKey::ARROW_UP},
      {VKEY_APPS, DomKey::CONTEXT_MENU},
      {VKEY_DBE_SBCSCHAR, DomKey::EISU},
      {VKEY_KANJI, DomKey::KANJI_MODE},
  });

  auto it = kMap.find(key_code);
  if (it != kMap.end()) {
    return it->second;
  }

  return DomKey::NONE;
}

DomKey DomKeyFromNsCharCode(char32_t char_code) {
  // Refer to:
  // https://github.com/WebKit/WebKit/blob/main/Source/WebCore/platform/ios/KeyEventCodesIOS.h#L40
  constexpr auto kMap = base::MakeFixedFlatMap<char32_t, DomKey>(
      {{0xF700, DomKey::ARROW_UP},
       {0xF701, DomKey::ARROW_DOWN},
       {0xF702, DomKey::ARROW_LEFT},
       {0xF703, DomKey::ARROW_RIGHT},
       {0xF704, DomKey::F1},
       {0xF705, DomKey::F2},
       {0xF706, DomKey::F3},
       {0xF707, DomKey::F4},
       {0xF708, DomKey::F5},
       {0xF709, DomKey::F6},
       {0xF70A, DomKey::F7},
       {0xF70B, DomKey::F8},
       {0xF70C, DomKey::F9},
       {0xF70D, DomKey::F10},
       {0xF70E, DomKey::F11},
       {0xF70F, DomKey::F12},
       {0xF710, DomKey::F13},
       {0xF711, DomKey::F14},
       {0xF712, DomKey::F15},
       {0xF713, DomKey::F16},
       {0xF714, DomKey::F17},
       {0xF715, DomKey::F18},
       {0xF716, DomKey::F19},
       {0xF717, DomKey::F20},
       {0xF718, DomKey::F21},
       {0xF719, DomKey::F22},
       {0xF71A, DomKey::F23},
       {0xF71B, DomKey::F24},
       // those keys that be commented out are not defined in DomKey yet:
       // {0xF71C, DomKey::F25},
       // {0xF71D, DomKey::F26},
       // {0xF71E, DomKey::F27},
       // {0xF71F, DomKey::F28},
       // {0xF720, DomKey::F29},
       // {0xF721, DomKey::F30},
       // {0xF722, DomKey::F31},
       // {0xF723, DomKey::F32},
       // {0xF724, DomKey::F33},
       // {0xF725, DomKey::F34},
       // {0xF726, DomKey::F35},
       {0xF727, DomKey::INSERT},
       {0xF728, DomKey::DEL},
       {0xF729, DomKey::HOME},
       // {0xF72A, DomKey::BEGIN},
       {0xF72B, DomKey::END},
       {0xF72C, DomKey::PAGE_UP},
       {0xF72D, DomKey::PAGE_DOWN},
       {0xF72E, DomKey::PRINT_SCREEN},
       {0xF72F, DomKey::SCROLL_LOCK},
       {0xF730, DomKey::PAUSE},
       // {0xF731, DomKey::SYS_REQ},
       // {0xF732, DomKey::BREAK},
       // {0xF733, DomKey::RESET},
       {0xF734, DomKey::MEDIA_STOP},
       {0xF735, DomKey::ALT},
       // {0xF736, DomKey::USER},
       // {0xF737, DomKey::SYSTEM},
       {0xF738, DomKey::PRINT},
       // {0xF739, DomKey::CLEAR_LINE},
       {0xF73A, DomKey::CLEAR},
       // {0xF73B, DomKey::INSERT_LINE},
       // {0xF73C, DomKey::DELETE_LINE},
       // {0xF73D, DomKey::INSERT_CHAR},
       // {0xF73E, DomKey::DELETE_CHAR},
       {0xF73F, DomKey::NAVIGATE_PREVIOUS},
       {
           0xF740,
           DomKey::NAVIGATE_NEXT,
       },
       {0xF741, DomKey::SELECT},
       {0xF742, DomKey::EXECUTE},
       {0xF743, DomKey::UNDO},
       {0xF744, DomKey::REDO},
       {0xF745, DomKey::FIND},
       {0xF746, DomKey::HELP},
       {0xF747, DomKey::MODE_CHANGE}});

  auto it = kMap.find(char_code);
  if (it != kMap.end()) {
    return it->second;
  }
  return DomKey::FromCharacter(char_code);
}

// Returns the last Unicode character from the given string.
char32_t ReadLastUnicodeCharacter(NSString* characters) {
  if (characters.length == 0) {
    return 0;
  }
  // Use uint16_t instead of char16_t to suppress a compiler warning.
  uint16_t trail = [characters characterAtIndex:characters.length - 1];
  if (CBU16_IS_SINGLE(trail)) {
    return static_cast<char32_t>(trail);
  }
  if (characters.length == 1 || !CBU16_IS_TRAIL(trail)) {
    return 0;
  }
  uint16_t lead = [characters characterAtIndex:characters.length - 2];
  if (!CBU16_IS_LEAD(lead)) {
    return 0;
  }
  return CBU16_GET_SUPPLEMENTARY(lead, trail);
}

}  // namespace

#if BUILDFLAG(IS_IOS_TVOS)
KeyboardCode KeyboardCodeFromUIKeyCode(UIKeyboardHIDUsage key_code) {
  // Refer to:
  // https://developer.apple.com/documentation/uikit/uikeyboardhidusage?language=objc
  constexpr auto kMap =
      base::MakeFixedFlatMap<UIKeyboardHIDUsage, KeyboardCode>(
          {{UIKeyboardHIDUsageKeyboardLeftArrow, KeyboardCode::VKEY_LEFT},
           {UIKeyboardHIDUsageKeyboardRightArrow, KeyboardCode::VKEY_RIGHT},
           {UIKeyboardHIDUsageKeyboardUpArrow, KeyboardCode::VKEY_UP},
           {UIKeyboardHIDUsageKeyboardDownArrow, KeyboardCode::VKEY_DOWN},
           {UIKeyboardHIDUsageKeyboardHome, KeyboardCode::VKEY_HOME},
           {UIKeyboardHIDUsageKeyboardEnd, KeyboardCode::VKEY_END},
           {UIKeyboardHIDUsageKeyboardDeleteForward, KeyboardCode::VKEY_DELETE},
           {UIKeyboardHIDUsageKeyboardDeleteOrBackspace,
            KeyboardCode::VKEY_BACK},
           {UIKeyboardHIDUsageKeyboardEscape, KeyboardCode::VKEY_ESCAPE},
           {UIKeyboardHIDUsageKeyboardInsert, KeyboardCode::VKEY_INSERT},
           {UIKeyboardHIDUsageKeyboardReturn, KeyboardCode::VKEY_RETURN},
           {UIKeyboardHIDUsageKeyboardReturnOrEnter, KeyboardCode::VKEY_RETURN},
           {UIKeyboardHIDUsageKeyboardTab, KeyboardCode::VKEY_TAB},
           {UIKeyboardHIDUsageKeyboardF1, KeyboardCode::VKEY_F1},
           {UIKeyboardHIDUsageKeyboardF2, KeyboardCode::VKEY_F2},
           {UIKeyboardHIDUsageKeyboardF3, KeyboardCode::VKEY_F3},
           {UIKeyboardHIDUsageKeyboardF4, KeyboardCode::VKEY_F4},
           {UIKeyboardHIDUsageKeyboardF5, KeyboardCode::VKEY_F5},
           {UIKeyboardHIDUsageKeyboardF6, KeyboardCode::VKEY_F6},
           {UIKeyboardHIDUsageKeyboardF7, KeyboardCode::VKEY_F7},
           {UIKeyboardHIDUsageKeyboardF8, KeyboardCode::VKEY_F8},
           {UIKeyboardHIDUsageKeyboardF9, KeyboardCode::VKEY_F9},
           {UIKeyboardHIDUsageKeyboardF10, KeyboardCode::VKEY_F10},
           {UIKeyboardHIDUsageKeyboardF11, KeyboardCode::VKEY_F11},
           {UIKeyboardHIDUsageKeyboardF12, KeyboardCode::VKEY_F12},
           {UIKeyboardHIDUsageKeyboardF13, KeyboardCode::VKEY_F13},
           {UIKeyboardHIDUsageKeyboardF14, KeyboardCode::VKEY_F14},
           {UIKeyboardHIDUsageKeyboardF15, KeyboardCode::VKEY_F15},
           {UIKeyboardHIDUsageKeyboardF16, KeyboardCode::VKEY_F16},
           {UIKeyboardHIDUsageKeyboardF17, KeyboardCode::VKEY_F17},
           {UIKeyboardHIDUsageKeyboardF18, KeyboardCode::VKEY_F18},
           {UIKeyboardHIDUsageKeyboardF19, KeyboardCode::VKEY_F19},
           {UIKeyboardHIDUsageKeyboardF20, KeyboardCode::VKEY_F20},
           {UIKeyboardHIDUsageKeyboardF21, KeyboardCode::VKEY_F21},
           {UIKeyboardHIDUsageKeyboardF22, KeyboardCode::VKEY_F22},
           {UIKeyboardHIDUsageKeyboardF23, KeyboardCode::VKEY_F23},
           {UIKeyboardHIDUsageKeyboardF24, KeyboardCode::VKEY_F24}});

  auto it = kMap.find(key_code);
  if (it != kMap.end()) {
    return it->second;
  }
  return KeyboardCode::VKEY_UNKNOWN;
}

DomCode DomCodeFromUIPress(UIPress* press, KeyboardCode key_code) {
  // TODO(https://crbug.com/391914246): Fix the assumption of the keyboard
  // layout being the US layout.
  DomKey dom_key = DomKeyFromKeyboardCode(press, key_code);
  return ui::UsLayoutDomKeyToDomCode(dom_key);
}

DomKey DomKeyFromKeyboardCode(UIPress* press, KeyboardCode key_code) {
  // TODO(https://crbug.com/391914246): Need to complete the implementation.
  NSString* characters =
      press.key.characters.precomposedStringWithCanonicalMapping;
  NSString* prefix_to_remove = @"UIKeyboardHIDUsageKeyboard";
  // Remove `prefix_to_remove` from `characters` in order to get a character
  // that can be utilized for detecting DomKey.
  NSString* updated_characters =
      [characters stringByReplacingOccurrencesOfString:prefix_to_remove
                                            withString:@""];
  char32_t character = ReadLastUnicodeCharacter(updated_characters);
  // Get DomKey from `character` only when the string length after removing the
  // prefix is 1. (e.g., the last character, 'A', in
  // "UIKeyboardHIDUsageKeyboardA" is useful to get DomKey from character code,
  // but the last character, 'w', in "UIKeyboardHIDUsageKeyboardUpArrow" for the
  // up arrow key is not useful for getting DomKey.)
  if ([updated_characters length] == 1 && IsDomKeyUnicodeCharacter(character)) {
    return DomKeyFromNsCharCode(character);
  }
  // Map non-character keys based on the physical key identifier.
  return DomKeyFromKeyCode(key_code);
}
#else
DomCode DomCodeFromBEKeyEntry(BEKeyEntry* entry) {
  // TODO(https://crbug.com/388320178): Fix the assumption of the keyboard
  // layout being the US layout.
  DomKey dom_key = DomKeyFromBEKeyEntry(entry);
  return ui::UsLayoutDomKeyToDomCode(dom_key);
}

DomKey DomKeyFromBEKeyEntry(BEKeyEntry* entry) {
  // TODO(https://crbug.com/388320178): Need to complete the implementation
  // According to Mac and Android's implementation, we might need to handle
  // the following cases:
  // 1. dead keys
  // 2. modifier flags

  NSString* characters =
      entry.key.characters.precomposedStringWithCanonicalMapping;
  char32_t character = ReadLastUnicodeCharacter(characters);
  if (IsDomKeyUnicodeCharacter(character)) {
    return DomKeyFromNsCharCode(character);
  }
  // Map non-character keys based on the physical key identifier.
  return DomKeyFromKeyCode(entry.key.keyCode);
}
#endif  // BUILDFLAG(IS_IOS_TVOS)

}  // namespace ui
