// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#import "ui/events/keycodes/keyboard_code_conversion_mac.h"

#import <Carbon/Carbon.h>

#include <algorithm>

#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/third_party/icu/icu_utf.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

namespace {

// A glyph modifier key is any of the following modifier keys: Shift, CapsLock
// or AltGr (Option for macOS). These keys may, when applied, cause a
// character-key to generate a different character.
//
// See https://w3c.github.io/uievents-key/#selecting-key-attribute-values.
constexpr int kGlyphModifiers = NSEventModifierFlagShift |
                                NSEventModifierFlagCapsLock |
                                NSEventModifierFlagOption;

// Per Apple docs, the buffer length can be up to 255 but is rarely more than 4.
// https://developer.apple.com/documentation/coreservices/1390584-uckeytranslate
constexpr int kUCKeyTranslateBufferLength = 4;

// Returns whether the given key is a valid DOM key character.
//
// See https://w3c.github.io/uievents-key/#key-string.
inline bool IsDomKeyUnicodeCharacter(char32_t c) {
  return base::IsValidCodepoint(c) && !base::IsUnicodeControl(c);
}

bool IsKeypadOrNumericKeyEvent(NSEvent* event) {
  // Check that this is the type of event that has a keyCode.
  switch (event.type) {
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
    case NSEventTypeFlagsChanged:
      break;
    default:
      return false;
  }

  switch (event.keyCode) {
    case kVK_ANSI_KeypadClear:
    case kVK_ANSI_KeypadEquals:
    case kVK_ANSI_KeypadMultiply:
    case kVK_ANSI_KeypadDivide:
    case kVK_ANSI_KeypadMinus:
    case kVK_ANSI_KeypadPlus:
    case kVK_ANSI_KeypadEnter:
    case kVK_ANSI_KeypadDecimal:
    case kVK_ANSI_Keypad0:
    case kVK_ANSI_Keypad1:
    case kVK_ANSI_Keypad2:
    case kVK_ANSI_Keypad3:
    case kVK_ANSI_Keypad4:
    case kVK_ANSI_Keypad5:
    case kVK_ANSI_Keypad6:
    case kVK_ANSI_Keypad7:
    case kVK_ANSI_Keypad8:
    case kVK_ANSI_Keypad9:
    case kVK_ANSI_0:
    case kVK_ANSI_1:
    case kVK_ANSI_2:
    case kVK_ANSI_3:
    case kVK_ANSI_4:
    case kVK_ANSI_5:
    case kVK_ANSI_6:
    case kVK_ANSI_7:
    case kVK_ANSI_8:
    case kVK_ANSI_9:
      return true;
  }

  return false;
}

// A convenient array for getting symbol characters on the number keys.
const char kShiftCharsForNumberKeys[] = ")!@#$%^&*(";

DomKey DomKeyFromKeyCode(unsigned short key_code) {
  constexpr auto kMap = base::MakeFixedFlatMap<unsigned short, DomKey>({
      {kVK_ANSI_KeypadEnter, DomKey::ENTER},
      {kVK_Return, DomKey::ENTER},
      {kVK_Tab, DomKey::TAB},
      {kVK_Delete, DomKey::BACKSPACE},
      {kVK_Escape, DomKey::ESCAPE},
      {kVK_Command, DomKey::META},
      {kVK_RightCommand, DomKey::META},
      {kVK_Shift, DomKey::SHIFT},
      {kVK_RightShift, DomKey::SHIFT},
      {kVK_CapsLock, DomKey::CAPS_LOCK},
      {kVK_Option, DomKey::ALT},
      {kVK_RightOption, DomKey::ALT},
      {kVK_Control, DomKey::CONTROL},
      {kVK_RightControl, DomKey::CONTROL},
      {kVK_Function, DomKey::FN},
      {kVK_VolumeUp, DomKey::AUDIO_VOLUME_UP},
      {kVK_VolumeDown, DomKey::AUDIO_VOLUME_DOWN},
      {kVK_Mute, DomKey::AUDIO_VOLUME_MUTE},
      {kVK_F1, DomKey::F1},
      {kVK_F2, DomKey::F2},
      {kVK_F3, DomKey::F3},
      {kVK_F4, DomKey::F4},
      {kVK_F5, DomKey::F5},
      {kVK_F6, DomKey::F6},
      {kVK_F7, DomKey::F7},
      {kVK_F8, DomKey::F8},
      {kVK_F9, DomKey::F9},
      {kVK_F10, DomKey::F10},
      {kVK_F11, DomKey::F11},
      {kVK_F12, DomKey::F12},
      {kVK_F13, DomKey::F13},
      {kVK_F14, DomKey::F14},
      {kVK_F15, DomKey::F15},
      {kVK_F16, DomKey::F16},
      {kVK_F17, DomKey::F17},
      {kVK_F18, DomKey::F18},
      {kVK_F19, DomKey::F19},
      {kVK_F20, DomKey::F20},
      {kVK_Help, DomKey::HELP},
      {kVK_Home, DomKey::HOME},
      {kVK_PageUp, DomKey::PAGE_UP},
      {kVK_ForwardDelete, DomKey::DEL},
      {kVK_End, DomKey::END},
      {kVK_PageDown, DomKey::PAGE_DOWN},
      {kVK_LeftArrow, DomKey::ARROW_LEFT},
      {kVK_RightArrow, DomKey::ARROW_RIGHT},
      {kVK_DownArrow, DomKey::ARROW_DOWN},
      {kVK_UpArrow, DomKey::ARROW_UP},
      {kVK_ContextualMenu, DomKey::CONTEXT_MENU},
      {kVK_JIS_Eisu, DomKey::EISU},
      {kVK_JIS_Kana, DomKey::KANJI_MODE},
  });

  auto it = kMap.find(key_code);
  if (it != kMap.end()) {
    return it->second;
  }

  return DomKey::NONE;
}

DomKey DomKeyFromNsCharCode(char32_t char_code) {
  constexpr auto kMap = base::MakeFixedFlatMap<char32_t, DomKey>({
      {NSUpArrowFunctionKey, DomKey::ARROW_UP},
      {NSDownArrowFunctionKey, DomKey::ARROW_DOWN},
      {NSLeftArrowFunctionKey, DomKey::ARROW_LEFT},
      {NSRightArrowFunctionKey, DomKey::ARROW_RIGHT},
      {NSF1FunctionKey, DomKey::F1},
      {NSF2FunctionKey, DomKey::F2},
      {NSF3FunctionKey, DomKey::F3},
      {NSF4FunctionKey, DomKey::F4},
      {NSF5FunctionKey, DomKey::F5},
      {NSF6FunctionKey, DomKey::F6},
      {NSF7FunctionKey, DomKey::F7},
      {NSF8FunctionKey, DomKey::F8},
      {NSF9FunctionKey, DomKey::F9},
      {NSF10FunctionKey, DomKey::F10},
      {NSF11FunctionKey, DomKey::F11},
      {NSF12FunctionKey, DomKey::F12},
      {NSF13FunctionKey, DomKey::F13},
      {NSF14FunctionKey, DomKey::F14},
      {NSF15FunctionKey, DomKey::F15},
      {NSF16FunctionKey, DomKey::F16},
      {NSF17FunctionKey, DomKey::F17},
      {NSF18FunctionKey, DomKey::F18},
      {NSF19FunctionKey, DomKey::F19},
      {NSF20FunctionKey, DomKey::F20},
      {NSF21FunctionKey, DomKey::F21},
      {NSF22FunctionKey, DomKey::F22},
      {NSF23FunctionKey, DomKey::F23},
      {NSF24FunctionKey, DomKey::F24},
      {NSInsertFunctionKey, DomKey::INSERT},
      {NSDeleteFunctionKey, DomKey::DEL},
      {NSHomeFunctionKey, DomKey::HOME},
      {NSEndFunctionKey, DomKey::END},
      {NSPageUpFunctionKey, DomKey::PAGE_UP},
      {NSPageDownFunctionKey, DomKey::PAGE_DOWN},
      {NSPrintScreenFunctionKey, DomKey::PRINT_SCREEN},
      {NSScrollLockFunctionKey, DomKey::SCROLL_LOCK},
      {NSPauseFunctionKey, DomKey::PAUSE},
      {NSPrintFunctionKey, DomKey::PRINT},
      {NSClearLineFunctionKey, DomKey::CLEAR},
      {NSSelectFunctionKey, DomKey::SELECT},
      {NSExecuteFunctionKey, DomKey::EXECUTE},
      {NSUndoFunctionKey, DomKey::UNDO},
      {NSRedoFunctionKey, DomKey::REDO},
      {NSFindFunctionKey, DomKey::FIND},
      {NSHelpFunctionKey, DomKey::HELP},
  });

  auto it = kMap.find(char_code);
  if (it != kMap.end()) {
    return it->second;
  }

  return DomKey::FromCharacter(char_code);
}

// Returns a macOS key code and modifier to a character based on the current
// keyboard layout.
//
// NsKeyCodeAndModifiersToCharacter() is efficient (around 6E-4 ms).
std::tuple<UniChar, bool> NsKeyCodeAndModifiersToCharacter(
    unsigned short key_code,
    int modifiers) {
  // Convert NSEvent modifiers to format UCKeyTranslate accepts. See docs
  // on UCKeyTranslate for more info.
  int unicode_modifiers = 0;
  if (modifiers & NSEventModifierFlagShift) {
    unicode_modifiers |= shiftKey;
  }
  if (modifiers & NSEventModifierFlagCapsLock) {
    unicode_modifiers |= alphaLock;
  }
  // if (modifiers & NSEventModifierFlagControl)
  //   unicode_modifiers |= controlKey;
  if (modifiers & NSEventModifierFlagOption) {
    unicode_modifiers |= optionKey;
  }
  // if (modifiers & NSEventModifierFlagCommand)
  //   unicode_modifiers |= cmdKey;
  UInt32 modifier_key_state = (unicode_modifiers >> 8) & 0xFF;

  UInt32 dead_key_state = 0;
  base::apple::ScopedCFTypeRef<TISInputSourceRef> input_source(
      TISCopyCurrentKeyboardLayoutInputSource());
  UniChar translated_char = TranslatedUnicodeCharFromKeyCode(
      input_source.get(), static_cast<UInt16>(key_code), kUCKeyActionDown,
      modifier_key_state, LMGetKbdLast(), &dead_key_state);

  bool is_dead_key = dead_key_state != 0;
  if (is_dead_key) {
    translated_char = TranslatedUnicodeCharFromKeyCode(
        input_source.get(), static_cast<UInt16>(kVK_Space), kUCKeyActionDown, 0,
        LMGetKbdLast(), &dead_key_state);
  }

  return {translated_char, is_dead_key};
}

// Returns the last Unicode character from the given string.
char32_t ReadLastUnicodeCharacter(NSString* characters) {
  if (characters.length == 0) {
    return 0;
  }
  char16_t trail = [characters characterAtIndex:characters.length - 1];
  if (CBU16_IS_SINGLE(trail)) {
    return trail;
  }
  if (characters.length == 1 || !CBU16_IS_TRAIL(trail)) {
    return 0;
  }
  char16_t lead = [characters characterAtIndex:characters.length - 2];
  if (!CBU16_IS_LEAD(lead)) {
    return 0;
  }
  return CBU16_GET_SUPPLEMENTARY(lead, trail);
}

}  // namespace

int MacKeyCodeForWindowsKeyCode(KeyboardCode keycode,
                                NSUInteger flags,
                                unichar* us_keyboard_shifted_character,
                                unichar* keyboard_character) {
  struct MacKeyCodeInfo {
    int mac_keycode;
    unichar character_ignoring_all_modifiers;
  };

  // TODO(suzhe): This map is not complete, missing entries have mac_keycode ==
  // -1.
  constexpr auto kMap = base::MakeFixedFlatMap<KeyboardCode, MacKeyCodeInfo>({
      {VKEY_BACK /* 0x08 */, {kVK_Delete, kBackspaceCharCode}},
      {VKEY_TAB /* 0x09 */, {kVK_Tab, kTabCharCode}},
      {VKEY_BACKTAB /* 0x0A */, {0x21E4, '\031'}},
      {VKEY_CLEAR /* 0x0C */, {kVK_ANSI_KeypadClear, kClearCharCode}},
      {VKEY_RETURN /* 0x0D */, {kVK_Return, kReturnCharCode}},
      {VKEY_SHIFT /* 0x10 */, {kVK_Shift, 0}},
      {VKEY_CONTROL /* 0x11 */, {kVK_Control, 0}},
      {VKEY_MENU /* 0x12 */, {kVK_Option, 0}},
      {VKEY_PAUSE /* 0x13 */, {-1, NSPauseFunctionKey}},
      {VKEY_CAPITAL /* 0x14 */, {kVK_CapsLock, 0}},
      {VKEY_KANA /* 0x15 (aka VKEY_HANGUL) */, {kVK_JIS_Kana, 0}},
      {VKEY_JUNJA /* 0x17 */, {-1, 0}},
      {VKEY_FINAL /* 0x18 */, {-1, 0}},
      {VKEY_HANJA /* 0x19 (aka VKEY_HANJI) */, {-1, 0}},
      {VKEY_ESCAPE /* 0x1B */, {kVK_Escape, kEscapeCharCode}},
      {VKEY_CONVERT /* 0x1C */, {-1, 0}},
      {VKEY_NONCONVERT /* 0x1D */, {-1, 0}},
      {VKEY_ACCEPT /* 0x1E */, {-1, 0}},
      {VKEY_MODECHANGE /* 0x1F */, {-1, 0}},
      {VKEY_SPACE /* 0x20 */, {kVK_Space, kSpaceCharCode}},
      {VKEY_PRIOR /* 0x21 */, {kVK_PageUp, NSPageUpFunctionKey}},
      {VKEY_NEXT /* 0x22 */, {kVK_PageDown, NSPageDownFunctionKey}},
      {VKEY_END /* 0x23 */, {kVK_End, NSEndFunctionKey}},
      {VKEY_HOME /* 0x24 */, {kVK_Home, NSHomeFunctionKey}},
      {VKEY_LEFT /* 0x25 */, {kVK_LeftArrow, NSLeftArrowFunctionKey}},
      {VKEY_UP /* 0x26 */, {kVK_UpArrow, NSUpArrowFunctionKey}},
      {VKEY_RIGHT /* 0x27 */, {kVK_RightArrow, NSRightArrowFunctionKey}},
      {VKEY_DOWN /* 0x28 */, {kVK_DownArrow, NSDownArrowFunctionKey}},
      {VKEY_SELECT /* 0x29 */, {-1, 0}},
      {VKEY_PRINT /* 0x2A */, {-1, NSPrintFunctionKey}},
      {VKEY_EXECUTE /* 0x2B */, {-1, NSExecuteFunctionKey}},
      {VKEY_SNAPSHOT /* 0x2C */, {-1, NSPrintScreenFunctionKey}},
      {VKEY_INSERT /* 0x2D */, {kVK_Help, NSInsertFunctionKey}},
      {VKEY_DELETE /* 0x2E */, {kVK_ForwardDelete, NSDeleteFunctionKey}},
      {VKEY_HELP /* 0x2F */, {kVK_Help, kHelpCharCode}},
      {VKEY_0 /* 0x30 */, {kVK_ANSI_0, '0'}},
      {VKEY_1 /* 0x31 */, {kVK_ANSI_1, '1'}},
      {VKEY_2 /* 0x32 */, {kVK_ANSI_2, '2'}},
      {VKEY_3 /* 0x33 */, {kVK_ANSI_3, '3'}},
      {VKEY_4 /* 0x34 */, {kVK_ANSI_4, '4'}},
      {VKEY_5 /* 0x35 */, {kVK_ANSI_5, '5'}},
      {VKEY_6 /* 0x36 */, {kVK_ANSI_6, '6'}},
      {VKEY_7 /* 0x37 */, {kVK_ANSI_7, '7'}},
      {VKEY_8 /* 0x38 */, {kVK_ANSI_8, '8'}},
      {VKEY_9 /* 0x39 */, {kVK_ANSI_9, '9'}},
      {VKEY_A /* 0x41 */, {kVK_ANSI_A, 'a'}},
      {VKEY_B /* 0x42 */, {kVK_ANSI_B, 'b'}},
      {VKEY_C /* 0x43 */, {kVK_ANSI_C, 'c'}},
      {VKEY_D /* 0x44 */, {kVK_ANSI_D, 'd'}},
      {VKEY_E /* 0x45 */, {kVK_ANSI_E, 'e'}},
      {VKEY_F /* 0x46 */, {kVK_ANSI_F, 'f'}},
      {VKEY_G /* 0x47 */, {kVK_ANSI_G, 'g'}},
      {VKEY_H /* 0x48 */, {kVK_ANSI_H, 'h'}},
      {VKEY_I /* 0x49 */, {kVK_ANSI_I, 'i'}},
      {VKEY_J /* 0x4A */, {kVK_ANSI_J, 'j'}},
      {VKEY_K /* 0x4B */, {kVK_ANSI_K, 'k'}},
      {VKEY_L /* 0x4C */, {kVK_ANSI_L, 'l'}},
      {VKEY_M /* 0x4D */, {kVK_ANSI_M, 'm'}},
      {VKEY_N /* 0x4E */, {kVK_ANSI_N, 'n'}},
      {VKEY_O /* 0x4F */, {kVK_ANSI_O, 'o'}},
      {VKEY_P /* 0x50 */, {kVK_ANSI_P, 'p'}},
      {VKEY_Q /* 0x51 */, {kVK_ANSI_Q, 'q'}},
      {VKEY_R /* 0x52 */, {kVK_ANSI_R, 'r'}},
      {VKEY_S /* 0x53 */, {kVK_ANSI_S, 's'}},
      {VKEY_T /* 0x54 */, {kVK_ANSI_T, 't'}},
      {VKEY_U /* 0x55 */, {kVK_ANSI_U, 'u'}},
      {VKEY_V /* 0x56 */, {kVK_ANSI_V, 'v'}},
      {VKEY_W /* 0x57 */, {kVK_ANSI_W, 'w'}},
      {VKEY_X /* 0x58 */, {kVK_ANSI_X, 'x'}},
      {VKEY_Y /* 0x59 */, {kVK_ANSI_Y, 'y'}},
      {VKEY_Z /* 0x5A */, {kVK_ANSI_Z, 'z'}},
      {VKEY_LWIN /* 0x5B */, {kVK_Command, 0}},
      {VKEY_RWIN /* 0x5C */, {kVK_RightCommand, 0}},
      {VKEY_APPS /* 0x5D */, {kVK_RightCommand, 0}},
      {VKEY_SLEEP /* 0x5F */, {-1, 0}},
      {VKEY_NUMPAD0 /* 0x60 */, {kVK_ANSI_Keypad0, '0'}},
      {VKEY_NUMPAD1 /* 0x61 */, {kVK_ANSI_Keypad1, '1'}},
      {VKEY_NUMPAD2 /* 0x62 */, {kVK_ANSI_Keypad2, '2'}},
      {VKEY_NUMPAD3 /* 0x63 */, {kVK_ANSI_Keypad3, '3'}},
      {VKEY_NUMPAD4 /* 0x64 */, {kVK_ANSI_Keypad4, '4'}},
      {VKEY_NUMPAD5 /* 0x65 */, {kVK_ANSI_Keypad5, '5'}},
      {VKEY_NUMPAD6 /* 0x66 */, {kVK_ANSI_Keypad6, '6'}},
      {VKEY_NUMPAD7 /* 0x67 */, {kVK_ANSI_Keypad7, '7'}},
      {VKEY_NUMPAD8 /* 0x68 */, {kVK_ANSI_Keypad8, '8'}},
      {VKEY_NUMPAD9 /* 0x69 */, {kVK_ANSI_Keypad9, '9'}},
      {VKEY_MULTIPLY /* 0x6A */, {kVK_ANSI_KeypadMultiply, '*'}},
      {VKEY_ADD /* 0x6B */, {kVK_ANSI_KeypadPlus, '+'}},
      {VKEY_SEPARATOR /* 0x6C */, {-1, 0}},
      {VKEY_SUBTRACT /* 0x6D */, {kVK_ANSI_KeypadMinus, '-'}},
      {VKEY_DECIMAL /* 0x6E */, {kVK_ANSI_KeypadDecimal, '.'}},
      {VKEY_DIVIDE /* 0x6F */, {kVK_ANSI_KeypadDivide, '/'}},
      {VKEY_F1 /* 0x70 */, {kVK_F1, NSF1FunctionKey}},
      {VKEY_F2 /* 0x71 */, {kVK_F2, NSF2FunctionKey}},
      {VKEY_F3 /* 0x72 */, {kVK_F3, NSF3FunctionKey}},
      {VKEY_F4 /* 0x73 */, {kVK_F4, NSF4FunctionKey}},
      {VKEY_F5 /* 0x74 */, {kVK_F5, NSF5FunctionKey}},
      {VKEY_F6 /* 0x75 */, {kVK_F6, NSF6FunctionKey}},
      {VKEY_F7 /* 0x76 */, {kVK_F7, NSF7FunctionKey}},
      {VKEY_F8 /* 0x77 */, {kVK_F8, NSF8FunctionKey}},
      {VKEY_F9 /* 0x78 */, {kVK_F9, NSF9FunctionKey}},
      {VKEY_F10 /* 0x79 */, {kVK_F10, NSF10FunctionKey}},
      {VKEY_F11 /* 0x7A */, {kVK_F11, NSF11FunctionKey}},
      {VKEY_F12 /* 0x7B */, {kVK_F12, NSF12FunctionKey}},
      {VKEY_F13 /* 0x7C */, {kVK_F13, NSF13FunctionKey}},
      {VKEY_F14 /* 0x7D */, {kVK_F14, NSF14FunctionKey}},
      {VKEY_F15 /* 0x7E */, {kVK_F15, NSF15FunctionKey}},
      {VKEY_F16 /* 0x7F */, {kVK_F16, NSF16FunctionKey}},
      {VKEY_F17 /* 0x80 */, {kVK_F17, NSF17FunctionKey}},
      {VKEY_F18 /* 0x81 */, {kVK_F18, NSF18FunctionKey}},
      {VKEY_F19 /* 0x82 */, {kVK_F19, NSF19FunctionKey}},
      {VKEY_F20 /* 0x83 */, {kVK_F20, NSF20FunctionKey}},
      {VKEY_F21 /* 0x84 */, {-1, NSF21FunctionKey}},
      {VKEY_F22 /* 0x85 */, {-1, NSF22FunctionKey}},
      {VKEY_F23 /* 0x86 */, {-1, NSF23FunctionKey}},
      {VKEY_F24 /* 0x87 */, {-1, NSF24FunctionKey}},
      {VKEY_NUMLOCK /* 0x90 */, {-1, 0}},
      {VKEY_SCROLL /* 0x91 */, {-1, NSScrollLockFunctionKey}},
      {VKEY_LSHIFT /* 0xA0 */, {kVK_Shift, 0}},
      {VKEY_RSHIFT /* 0xA1 */, {kVK_Shift, 0}},
      {VKEY_LCONTROL /* 0xA2 */, {kVK_Control, 0}},
      {VKEY_RCONTROL /* 0xA3 */, {kVK_Control, 0}},
      {VKEY_LMENU /* 0xA4 */, {-1, 0}},
      {VKEY_RMENU /* 0xA5 */, {-1, 0}},
      {VKEY_BROWSER_BACK /* 0xA6 */, {-1, 0}},
      {VKEY_BROWSER_FORWARD /* 0xA7 */, {-1, 0}},
      {VKEY_BROWSER_REFRESH /* 0xA8 */, {-1, 0}},
      {VKEY_BROWSER_STOP /* 0xA9 */, {-1, 0}},
      {VKEY_BROWSER_SEARCH /* 0xAA */, {-1, 0}},
      {VKEY_BROWSER_FAVORITES /* 0xAB */, {-1, 0}},
      {VKEY_BROWSER_HOME /* 0xAC */, {-1, 0}},
      {VKEY_VOLUME_MUTE /* 0xAD */, {-1, 0}},
      {VKEY_VOLUME_DOWN /* 0xAE */, {-1, 0}},
      {VKEY_VOLUME_UP /* 0xAF */, {-1, 0}},
      {VKEY_MEDIA_NEXT_TRACK /* 0xB0 */, {-1, 0}},
      {VKEY_MEDIA_PREV_TRACK /* 0xB1 */, {-1, 0}},
      {VKEY_MEDIA_STOP /* 0xB2 */, {-1, 0}},
      {VKEY_MEDIA_PLAY_PAUSE /* 0xB3 */, {-1, 0}},
      {VKEY_MEDIA_LAUNCH_MAIL /* 0xB4 */, {-1, 0}},
      {VKEY_MEDIA_LAUNCH_MEDIA_SELECT /* 0xB5 */, {-1, 0}},
      {VKEY_MEDIA_LAUNCH_APP1 /* 0xB6 */, {-1, 0}},
      {VKEY_MEDIA_LAUNCH_APP2 /* 0xB7 */, {-1, 0}},
      {VKEY_OEM_1 /* 0xBA */, {kVK_ANSI_Semicolon, ';'}},
      {VKEY_OEM_PLUS /* 0xBB */, {kVK_ANSI_Equal, '='}},
      {VKEY_OEM_COMMA /* 0xBC */, {kVK_ANSI_Comma, ','}},
      {VKEY_OEM_MINUS /* 0xBD */, {kVK_ANSI_Minus, '-'}},
      {VKEY_OEM_PERIOD /* 0xBE */, {kVK_ANSI_Period, '.'}},
      {VKEY_OEM_2 /* 0xBF */, {kVK_ANSI_Slash, '/'}},
      {VKEY_OEM_3 /* 0xC0 */, {kVK_ANSI_Grave, '`'}},
      {VKEY_OEM_4 /* 0xDB */, {kVK_ANSI_LeftBracket, '['}},
      {VKEY_OEM_5 /* 0xDC */, {kVK_ANSI_Backslash, '\\'}},
      {VKEY_OEM_6 /* 0xDD */, {kVK_ANSI_RightBracket, ']'}},
      {VKEY_OEM_7 /* 0xDE */, {kVK_ANSI_Quote, '\''}},
      {VKEY_OEM_8 /* 0xDF */, {-1, 0}},
      {VKEY_OEM_102 /* 0xE2 */, {-1, 0}},
      {VKEY_PROCESSKEY /* 0xE5 */, {-1, 0}},
      {VKEY_PACKET /* 0xE7 */, {-1, 0}},
      {VKEY_ATTN /* 0xF6 */, {-1, 0}},
      {VKEY_CRSEL /* 0xF7 */, {-1, 0}},
      {VKEY_EXSEL /* 0xF8 */, {-1, 0}},
      {VKEY_EREOF /* 0xF9 */, {-1, 0}},
      {VKEY_PLAY /* 0xFA */, {-1, 0}},
      {VKEY_ZOOM /* 0xFB */, {-1, 0}},
      {VKEY_NONAME /* 0xFC */, {-1, 0}},
      {VKEY_PA1 /* 0xFD */, {-1, 0}},
      {VKEY_OEM_CLEAR /* 0xFE */, {kVK_ANSI_KeypadClear, kClearCharCode}},
  });

  // In release code, |flags| is used to lookup accelerators, so logic to handle
  // caps lock properly isn't implemented.
  DCHECK_EQ(0u, flags & NSEventModifierFlagCapsLock);

  auto it = kMap.find(keycode);
  if (it == kMap.end() || it->second.mac_keycode == -1) {
    return -1;
  }

  int mac_keycode = it->second.mac_keycode;
  if (keyboard_character) {
    *keyboard_character = it->second.character_ignoring_all_modifiers;
  }

  if (!us_keyboard_shifted_character) {
    return mac_keycode;
  }

  *us_keyboard_shifted_character = it->second.character_ignoring_all_modifiers;

  // Fill in |us_keyboard_shifted_character| according to flags.
  if (flags & NSEventModifierFlagShift) {
    if (keycode >= VKEY_0 && keycode <= VKEY_9) {
      *us_keyboard_shifted_character =
          kShiftCharsForNumberKeys[keycode - VKEY_0];
    } else if (keycode >= VKEY_A && keycode <= VKEY_Z) {
      *us_keyboard_shifted_character = 'A' + (keycode - VKEY_A);
    } else {
      switch (mac_keycode) {
        case kVK_ANSI_Grave:
          *us_keyboard_shifted_character = '~';
          break;
        case kVK_ANSI_Minus:
          *us_keyboard_shifted_character = '_';
          break;
        case kVK_ANSI_Equal:
          *us_keyboard_shifted_character = '+';
          break;
        case kVK_ANSI_LeftBracket:
          *us_keyboard_shifted_character = '{';
          break;
        case kVK_ANSI_RightBracket:
          *us_keyboard_shifted_character = '}';
          break;
        case kVK_ANSI_Backslash:
          *us_keyboard_shifted_character = '|';
          break;
        case kVK_ANSI_Semicolon:
          *us_keyboard_shifted_character = ':';
          break;
        case kVK_ANSI_Quote:
          *us_keyboard_shifted_character = '\"';
          break;
        case kVK_ANSI_Comma:
          *us_keyboard_shifted_character = '<';
          break;
        case kVK_ANSI_Period:
          *us_keyboard_shifted_character = '>';
          break;
        case kVK_ANSI_Slash:
          *us_keyboard_shifted_character = '?';
          break;
        default:
          break;
      }
    }
  }

  // TODO(suzhe): Support characters for Option key bindings.
  return mac_keycode;
}

KeyboardCode KeyboardCodeFromCharCode(unichar char_code) {
  constexpr auto kMap = base::MakeFixedFlatMap<unichar, KeyboardCode>({
      {'a', VKEY_A},
      {'A', VKEY_A},
      {'b', VKEY_B},
      {'B', VKEY_B},
      {'c', VKEY_C},
      {'C', VKEY_C},
      {'d', VKEY_D},
      {'D', VKEY_D},
      {'e', VKEY_E},
      {'E', VKEY_E},
      {'f', VKEY_F},
      {'F', VKEY_F},
      {'g', VKEY_G},
      {'G', VKEY_G},
      {'h', VKEY_H},
      {'H', VKEY_H},
      {'i', VKEY_I},
      {'I', VKEY_I},
      {'j', VKEY_J},
      {'J', VKEY_J},
      {'k', VKEY_K},
      {'K', VKEY_K},
      {'l', VKEY_L},
      {'L', VKEY_L},
      {'m', VKEY_M},
      {'M', VKEY_M},
      {'n', VKEY_N},
      {'N', VKEY_N},
      {'o', VKEY_O},
      {'O', VKEY_O},
      {'p', VKEY_P},
      {'P', VKEY_P},
      {'q', VKEY_Q},
      {'Q', VKEY_Q},
      {'r', VKEY_R},
      {'R', VKEY_R},
      {'s', VKEY_S},
      {'S', VKEY_S},
      {'t', VKEY_T},
      {'T', VKEY_T},
      {'u', VKEY_U},
      {'U', VKEY_U},
      {'v', VKEY_V},
      {'V', VKEY_V},
      {'w', VKEY_W},
      {'W', VKEY_W},
      {'x', VKEY_X},
      {'X', VKEY_X},
      {'y', VKEY_Y},
      {'Y', VKEY_Y},
      {'z', VKEY_Z},
      {'Z', VKEY_Z},

      {'1', VKEY_1},
      {'2', VKEY_2},
      {'3', VKEY_3},
      {'4', VKEY_4},
      {'5', VKEY_5},
      {'6', VKEY_6},
      {'7', VKEY_7},
      {'8', VKEY_8},
      {'9', VKEY_9},
      {'0', VKEY_0},

      {NSPauseFunctionKey, VKEY_PAUSE},
      {NSSelectFunctionKey, VKEY_SELECT},
      {NSPrintFunctionKey, VKEY_PRINT},
      {NSExecuteFunctionKey, VKEY_EXECUTE},
      {NSPrintScreenFunctionKey, VKEY_SNAPSHOT},
      {NSInsertFunctionKey, VKEY_INSERT},
      {NSF21FunctionKey, VKEY_F21},
      {NSF22FunctionKey, VKEY_F22},
      {NSF23FunctionKey, VKEY_F23},
      {NSF24FunctionKey, VKEY_F24},
      {NSScrollLockFunctionKey, VKEY_SCROLL},

      // U.S. Specific mappings.  Mileage may vary.
      {';', VKEY_OEM_1},
      {':', VKEY_OEM_1},
      {'=', VKEY_OEM_PLUS},
      {'+', VKEY_OEM_PLUS},
      {',', VKEY_OEM_COMMA},
      {'<', VKEY_OEM_COMMA},
      {'-', VKEY_OEM_MINUS},
      {'_', VKEY_OEM_MINUS},
      {'.', VKEY_OEM_PERIOD},
      {'>', VKEY_OEM_PERIOD},
      {'/', VKEY_OEM_2},
      {'?', VKEY_OEM_2},
      {'`', VKEY_OEM_3},
      {'~', VKEY_OEM_3},
      {'[', VKEY_OEM_4},
      {'{', VKEY_OEM_4},
      {'\\', VKEY_OEM_5},
      {'|', VKEY_OEM_5},
      {']', VKEY_OEM_6},
      {'}', VKEY_OEM_6},
      {'\'', VKEY_OEM_7},
      {'"', VKEY_OEM_7},
  });

  auto it = kMap.find(char_code);
  if (it != kMap.end()) {
    return it->second;
  }

  return VKEY_UNKNOWN;
}

KeyboardCode KeyboardCodeFromKeyCode(unsigned short key_code) {
  static const KeyboardCode kKeyboardCodes[] = {
      /* 0x00 */ VKEY_A,
      /* 0x01 */ VKEY_S,
      /* 0x02 */ VKEY_D,
      /* 0x03 */ VKEY_F,
      /* 0x04 */ VKEY_H,
      /* 0x05 */ VKEY_G,
      /* 0x06 */ VKEY_Z,
      /* 0x07 */ VKEY_X,
      /* 0x08 */ VKEY_C,
      /* 0x09 */ VKEY_V,
      /* 0x0A */ VKEY_OEM_3,  // Section key.
      /* 0x0B */ VKEY_B,
      /* 0x0C */ VKEY_Q,
      /* 0x0D */ VKEY_W,
      /* 0x0E */ VKEY_E,
      /* 0x0F */ VKEY_R,
      /* 0x10 */ VKEY_Y,
      /* 0x11 */ VKEY_T,
      /* 0x12 */ VKEY_1,
      /* 0x13 */ VKEY_2,
      /* 0x14 */ VKEY_3,
      /* 0x15 */ VKEY_4,
      /* 0x16 */ VKEY_6,
      /* 0x17 */ VKEY_5,
      /* 0x18 */ VKEY_OEM_PLUS,  // =+
      /* 0x19 */ VKEY_9,
      /* 0x1A */ VKEY_7,
      /* 0x1B */ VKEY_OEM_MINUS,  // -_
      /* 0x1C */ VKEY_8,
      /* 0x1D */ VKEY_0,
      /* 0x1E */ VKEY_OEM_6,  // ]}
      /* 0x1F */ VKEY_O,
      /* 0x20 */ VKEY_U,
      /* 0x21 */ VKEY_OEM_4,  // {[
      /* 0x22 */ VKEY_I,
      /* 0x23 */ VKEY_P,
      /* 0x24 */ VKEY_RETURN,  // Return
      /* 0x25 */ VKEY_L,
      /* 0x26 */ VKEY_J,
      /* 0x27 */ VKEY_OEM_7,  // '"
      /* 0x28 */ VKEY_K,
      /* 0x29 */ VKEY_OEM_1,      // ;:
      /* 0x2A */ VKEY_OEM_5,      // \|
      /* 0x2B */ VKEY_OEM_COMMA,  // ,<
      /* 0x2C */ VKEY_OEM_2,      // /?
      /* 0x2D */ VKEY_N,
      /* 0x2E */ VKEY_M,
      /* 0x2F */ VKEY_OEM_PERIOD,  // .>
      /* 0x30 */ VKEY_TAB,
      /* 0x31 */ VKEY_SPACE,
      /* 0x32 */ VKEY_OEM_3,    // `~
      /* 0x33 */ VKEY_BACK,     // Backspace
      /* 0x34 */ VKEY_UNKNOWN,  // n/a
      /* 0x35 */ VKEY_ESCAPE,
      /* 0x36 */ VKEY_APPS,     // Right Command
      /* 0x37 */ VKEY_LWIN,     // Left Command
      /* 0x38 */ VKEY_SHIFT,    // Left Shift
      /* 0x39 */ VKEY_CAPITAL,  // Caps Lock
      /* 0x3A */ VKEY_MENU,     // Left Option
      /* 0x3B */ VKEY_CONTROL,  // Left Ctrl
      /* 0x3C */ VKEY_SHIFT,    // Right Shift
      /* 0x3D */ VKEY_MENU,     // Right Option
      /* 0x3E */ VKEY_CONTROL,  // Right Ctrl
      /* 0x3F */ VKEY_UNKNOWN,  // fn
      /* 0x40 */ VKEY_F17,
      /* 0x41 */ VKEY_DECIMAL,   // Num Pad .
      /* 0x42 */ VKEY_UNKNOWN,   // n/a
      /* 0x43 */ VKEY_MULTIPLY,  // Num Pad *
      /* 0x44 */ VKEY_UNKNOWN,   // n/a
      /* 0x45 */ VKEY_ADD,       // Num Pad +
      /* 0x46 */ VKEY_UNKNOWN,   // n/a
      /* 0x47 */ VKEY_CLEAR,     // Num Pad Clear
      /* 0x48 */ VKEY_VOLUME_UP,
      /* 0x49 */ VKEY_VOLUME_DOWN,
      /* 0x4A */ VKEY_VOLUME_MUTE,
      /* 0x4B */ VKEY_DIVIDE,    // Num Pad /
      /* 0x4C */ VKEY_RETURN,    // Num Pad Enter
      /* 0x4D */ VKEY_UNKNOWN,   // n/a
      /* 0x4E */ VKEY_SUBTRACT,  // Num Pad -
      /* 0x4F */ VKEY_F18,
      /* 0x50 */ VKEY_F19,
      /* 0x51 */ VKEY_OEM_PLUS,  // Num Pad =.
      /* 0x52 */ VKEY_NUMPAD0,
      /* 0x53 */ VKEY_NUMPAD1,
      /* 0x54 */ VKEY_NUMPAD2,
      /* 0x55 */ VKEY_NUMPAD3,
      /* 0x56 */ VKEY_NUMPAD4,
      /* 0x57 */ VKEY_NUMPAD5,
      /* 0x58 */ VKEY_NUMPAD6,
      /* 0x59 */ VKEY_NUMPAD7,
      /* 0x5A */ VKEY_F20,
      /* 0x5B */ VKEY_NUMPAD8,
      /* 0x5C */ VKEY_NUMPAD9,
      /* 0x5D */ VKEY_UNKNOWN,  // Yen (JIS Keyboard Only)
      /* 0x5E */ VKEY_UNKNOWN,  // Underscore (JIS Keyboard Only)
      /* 0x5F */ VKEY_UNKNOWN,  // KeypadComma (JIS Keyboard Only)
      /* 0x60 */ VKEY_F5,
      /* 0x61 */ VKEY_F6,
      /* 0x62 */ VKEY_F7,
      /* 0x63 */ VKEY_F3,
      /* 0x64 */ VKEY_F8,
      /* 0x65 */ VKEY_F9,
      /* 0x66 */ VKEY_UNKNOWN,  // Eisu (JIS Keyboard Only)
      /* 0x67 */ VKEY_F11,
      /* 0x68 */ VKEY_UNKNOWN,  // Kana (JIS Keyboard Only)
      /* 0x69 */ VKEY_F13,
      /* 0x6A */ VKEY_F16,
      /* 0x6B */ VKEY_F14,
      /* 0x6C */ VKEY_UNKNOWN,  // n/a
      /* 0x6D */ VKEY_F10,
      /* 0x6E */ VKEY_APPS,  // Context Menu key
      /* 0x6F */ VKEY_F12,
      /* 0x70 */ VKEY_UNKNOWN,  // n/a
      /* 0x71 */ VKEY_F15,
      /* 0x72 */ VKEY_INSERT,  // Help
      /* 0x73 */ VKEY_HOME,    // Home
      /* 0x74 */ VKEY_PRIOR,   // Page Up
      /* 0x75 */ VKEY_DELETE,  // Forward Delete
      /* 0x76 */ VKEY_F4,
      /* 0x77 */ VKEY_END,  // End
      /* 0x78 */ VKEY_F2,
      /* 0x79 */ VKEY_NEXT,  // Page Down
      /* 0x7A */ VKEY_F1,
      /* 0x7B */ VKEY_LEFT,    // Left Arrow
      /* 0x7C */ VKEY_RIGHT,   // Right Arrow
      /* 0x7D */ VKEY_DOWN,    // Down Arrow
      /* 0x7E */ VKEY_UP,      // Up Arrow
      /* 0x7F */ VKEY_UNKNOWN  // n/a
  };

  if (key_code >= 0x80) {
    return VKEY_UNKNOWN;
  }

  return kKeyboardCodes[key_code];
}

KeyboardCode KeyboardCodeFromNSEvent(NSEvent* event) {
  KeyboardCode code = VKEY_UNKNOWN;

  // Numeric keys 0-9 should always return |keyCode| 0-9.
  // https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/keyCode#Printable_keys_in_standard_position
  if (!IsKeypadOrNumericKeyEvent(event) &&
      (event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp)) {
    // Handles Dvorak-QWERTY Cmd case.
    // https://github.com/WebKit/webkit/blob/4d41c98b1de467f5d2a8fcba84d7c5268f11b0cc/Source/WebCore/platform/mac/PlatformEventFactoryMac.mm#L329
    NSString* characters = event.characters;
    if (characters.length > 0) {
      code = KeyboardCodeFromCharCode([characters characterAtIndex:0]);
    }
    if (code) {
      return code;
    }

    characters = event.charactersIgnoringModifiers;
    if (characters.length > 0) {
      code = KeyboardCodeFromCharCode([characters characterAtIndex:0]);
    }
    if (code) {
      return code;
    }
  }
  return KeyboardCodeFromKeyCode(event.keyCode);
}

int ISOKeyboardKeyCodeMap(int native_key_code) {
  // macOS will swap 'Backquote' and 'IntlBackslash' if it's an ISO keyboard.
  // https://crbug.com/600607
  switch (native_key_code) {
    case kVK_ISO_Section:
      return kVK_ANSI_Grave;
    case kVK_ANSI_Grave:
      return kVK_ISO_Section;
    default:
      return native_key_code;
  }
}

DomCode DomCodeFromNSEvent(NSEvent* event) {
  return ui::KeycodeConverter::NativeKeycodeToDomCode(event.keyCode);
}

// To select an appropriate key attribute value to store in a KeyboardEvent's
// key attribute, run these steps:
//
//  1. Let key be a DOMString initially set to "Unidentified".
//  2. If there exists an appropriate named key attribute value for this key
//     event, then set key to that named key attribute value.
//  3. Else, if the key event generates a valid key string, then set key to that
//     key string value.
//  4. Else, if the key event has any modifier keys other than glyph modifier
//     keys, then set key to the key string that would have been generated by
//     this event if it had been typed with all modifer keys removed except for
//     glyph modifier keys.
//  5. Return key as the key attribute value for this key event.
//
// Taken from https://w3c.github.io/uievents-key/#selecting-key-attribute-values
DomKey DomKeyFromNSEvent(NSEvent* event) {
  switch (event.type) {
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp: {
      // macOS Eisu Kana key events have a space symbol (U+0020) as
      // event.characters, but the symbol is not generated for users and the
      // event is just used for enabling/disabling an IME.
      if (event.keyCode == kVK_JIS_Eisu || event.keyCode == kVK_JIS_Kana) {
        return DomKeyFromKeyCode(event.keyCode);
      }

      // Step pre-3. Not specified in the spec, but we need special handling for
      // dead keys in macOS.
      auto [maybe_dead_key, is_dead_key] =
          NsKeyCodeAndModifiersToCharacter(event.keyCode, event.modifierFlags);
      if (is_dead_key) {
        return DomKey::DeadKeyFromCombiningCharacter(maybe_dead_key);
      }

      // Step 2 and 3. If there exists an appropriate named key attribute value
      // for this key event or the key event generates a valid key string, set
      // key to that named key attribute value or to the generated string value.
      NSString* characters =
          event.characters.precomposedStringWithCanonicalMapping;

      // When a dead key is pressed and the next key pressed generates an
      // invalid dead key sequences, that next key press will include the
      // previous dead key character at the beginning, so we always need to read
      // from the back.
      char32_t character = ReadLastUnicodeCharacter(characters);
      if (IsDomKeyUnicodeCharacter(character)) {
        return DomKeyFromNsCharCode(character);
      }

      // Step 4. If the key event has any modifier keys other than glyph
      // modifier keys, then set key to the key string that would have been
      // generated by this event if it had been typed with all modifier keys
      // removed except for glyph modifier keys.
      if ((event.modifierFlags & kGlyphModifiers) != event.modifierFlags) {
        character = std::get<UniChar>(NsKeyCodeAndModifiersToCharacter(
            event.keyCode, event.modifierFlags & kGlyphModifiers));
      }
      if (IsDomKeyUnicodeCharacter(character)) {
        return DomKeyFromNsCharCode(character);
      }
      // Map non-character keys based on the physical key identifier.
      return DomKeyFromKeyCode(event.keyCode);
    }
    case NSEventTypeFlagsChanged: {
      // This event does not generate characters, but its key code is a named
      // key.
      return DomKeyFromKeyCode(event.keyCode);
    }
    default:
      NOTREACHED();
  }
}

UniChar TranslatedUnicodeCharFromKeyCode(TISInputSourceRef input_source,
                                         UInt16 key_code,
                                         UInt16 key_action,
                                         UInt32 modifier_key_state,
                                         UInt32 keyboard_type,
                                         UInt32* dead_key_state) {
  DCHECK(dead_key_state);

  CFDataRef layout_data =
      base::apple::CFCast<CFDataRef>(TISGetInputSourceProperty(
          input_source, kTISPropertyUnicodeKeyLayoutData));
  if (!layout_data) {
    return 0xFFFD;  // REPLACEMENT CHARACTER
  }

  const UCKeyboardLayout* keyboard_layout =
      reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(layout_data));
  DCHECK(keyboard_layout);

  UniChar char_buffer[kUCKeyTranslateBufferLength] = {0};
  UniCharCount buffer_length = kUCKeyTranslateBufferLength;

  OSStatus ret = UCKeyTranslate(
      keyboard_layout, key_code, key_action, modifier_key_state, keyboard_type,
      kUCKeyTranslateNoDeadKeysBit, dead_key_state, kUCKeyTranslateBufferLength,
      &buffer_length, char_buffer);
  OSSTATUS_DCHECK(ret == noErr, ret);

  // TODO(input-dev): Handle multiple character case. Should be rare.
  return char_buffer[0];
}

}  // namespace ui
