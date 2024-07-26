// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/keycodes/dom/keycode_converter.h"

#include <string_view>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "build/build_config.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <linux/input.h>
#endif

namespace ui {

namespace {

// Table of USB codes (equivalent to DomCode values), native scan codes,
// and DOM Level 3 |code| strings.
#if BUILDFLAG(IS_WIN)
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  { usb, win, code }
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  { usb, xkb, code }
#elif BUILDFLAG(IS_APPLE)
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  { usb, mac, code }
#elif BUILDFLAG(IS_ANDROID)
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  { usb, evdev, code }
#elif BUILDFLAG(IS_FUCHSIA)
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  { usb, usb, code }
#else
#error Unsupported platform
#endif
#define DOM_CODE_DECLARATION constexpr KeycodeMapEntry kDomCodeMappings[] =
#include "ui/events/keycodes/dom/dom_code_data.inc"
#undef DOM_CODE
#undef DOM_CODE_DECLARATION

// Table of DomKey enum values and DOM Level 3 |key| strings.
struct DomKeyMapEntry {
  DomKey dom_key;
  const char* string;
};

#define DOM_KEY_MAP_DECLARATION_START \
  constexpr DomKeyMapEntry kDomKeyMappings[] = {
#define DOM_KEY_UNI(key, id, value) {DomKey::id, key},
#define DOM_KEY_MAP(key, id, value) {DomKey::id, key},
#define DOM_KEY_MAP_DECLARATION_END \
  }                                 \
  ;
#include "ui/events/keycodes/dom/dom_key_data.inc"
#undef DOM_KEY_MAP_DECLARATION_START
#undef DOM_KEY_MAP
#undef DOM_KEY_UNI
#undef DOM_KEY_MAP_DECLARATION_END

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// The offset between XKB Keycode and evdev code.
constexpr int kXkbKeycodeOffset = 8;

// TODO(crbug.com/40151699): After migrating native code for
// these platforms from XKB to evdev, use XKB_INVALID_KEYCODE
// (=0xFFFFFFFF) to represent invalid XKB keycode.
// Currently, 0 is returned for backward compatibility.

// Converts XKB keycode to evdev code, based on the mapping
// usually available at /usr/share/X11/xkb/keycodes/evdev.
// See also
// https://xkbcommon.org/doc/current/xkbcommon_8h.html#ac29aee92124c08d1953910ab28ee1997
// for the reference of the history of key mapping.
// Returns KEY_RESERVED for unknown XKB keycode mapping.
int XkbKeycodeToEvdevCode(uint32_t xkb_keycode) {
  // There's no mapping from XKB keycode in range [0-7] (inclusive)
  // to evdev. Return KEY_RESERVED as an error.
  if (xkb_keycode < kXkbKeycodeOffset)
    return KEY_RESERVED;
  return static_cast<int>(xkb_keycode - kXkbKeycodeOffset);
}

// Converts evdev code into XKB keycode.
// Returns KeycodeConverter::InvalidNativeKeycode() if the given code is in
// the invalid range or KEY_RESERVED.
uint32_t EvdevCodeToXkbKeycode(int evdev_code) {
  if (evdev_code < 0 || evdev_code > KEY_MAX || evdev_code == KEY_RESERVED)
    return KeycodeConverter::InvalidNativeKeycode();

  // TODO(crbug.com/40151699): Move this to EvdevCodeToDomCode on
  // migration.
  if (evdev_code == KEY_PLAYCD)
    evdev_code = KEY_PLAY;
  return static_cast<uint32_t>(evdev_code + kXkbKeycodeOffset);
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)

bool IsAlphaNumericKeyboardCode(KeyboardCode key_code) {
  return (key_code >= VKEY_A && key_code <= VKEY_Z) ||
         (key_code >= VKEY_0 && key_code <= VKEY_9);
}

bool ShouldPositionallyRemapKey(KeyboardCode key_code, DomCode code) {
  switch (code) {
    // Following set of keys should only be positionally remapped if the
    // original key_code is not an alphanumeric key. This is because there are
    // many layouts where the OEM keys in the US layout are instead
    // alphabetic keys in the other layout. In this case, the shortcut using the
    // key should be mapped to the semantic meaning (the letter/number) instead
    // of the position.

    // An example is the French layout where the semicolon key on the US layout
    // is the "m" key on the French keyboard. This prevents us from remapping
    // the "m" key on the French keyboard to "semicolon" for the purposes of
    // shortcuts.
    case DomCode::SEMICOLON:
    case DomCode::QUOTE:
    case DomCode::BACKSLASH:
    case DomCode::BACKQUOTE:
    case DomCode::INTL_BACKSLASH:
      return !IsAlphaNumericKeyboardCode(key_code);
    case DomCode::BRACKET_LEFT:
    case DomCode::BRACKET_RIGHT:
    case DomCode::MINUS:
    case DomCode::EQUAL:
    case DomCode::COMMA:
    case DomCode::PERIOD:
    case DomCode::SLASH:
    default:
      return true;
  }
}

#endif

}  // namespace

// static
size_t KeycodeConverter::NumKeycodeMapEntriesForTest() {
  return std::size(kDomCodeMappings);
}

// static
const KeycodeMapEntry* KeycodeConverter::GetKeycodeMapForTest() {
  return &kDomCodeMappings[0];
}

// static
const char* KeycodeConverter::DomKeyStringForTest(size_t index) {
  if (index >= std::size(kDomKeyMappings))
    return nullptr;
  return kDomKeyMappings[index].string;
}

// static
int KeycodeConverter::InvalidNativeKeycode() {
  return kDomCodeMappings[0].native_keycode;
}

// TODO(zijiehe): Most of the following functions can be optimized by using
// either multiple arrays or unordered_map.

// static
DomCode KeycodeConverter::NativeKeycodeToDomCode(int native_keycode) {
  for (auto& mapping : kDomCodeMappings) {
    if (mapping.native_keycode == native_keycode)
      return static_cast<DomCode>(mapping.usb_keycode);
  }
  return DomCode::NONE;
}

// static
int KeycodeConverter::DomCodeToNativeKeycode(DomCode code) {
  return UsbKeycodeToNativeKeycode(static_cast<uint32_t>(code));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// static
DomCode KeycodeConverter::XkbKeycodeToDomCode(uint32_t xkb_keycode) {
  // Currently XKB keycode is the native keycode.
  // TODO(crbug.com/40151699): Replace with evdev.
  return NativeKeycodeToDomCode(static_cast<int>(xkb_keycode));
}

// static
uint32_t KeycodeConverter::DomCodeToXkbKeycode(DomCode code) {
  // Currently XKB keycode is the native keycode.
  // TODO(crbug.com/40151699): Replace with evdev.
  return static_cast<uint32_t>(DomCodeToNativeKeycode(code));
}

// static
DomCode KeycodeConverter::EvdevCodeToDomCode(int evdev_code) {
  // Currently XKB keycode is the native keycode.
  // TODO(crbug.com/40151699): Replace with evdev.
  return XkbKeycodeToDomCode(EvdevCodeToXkbKeycode(evdev_code));
}

// static
int KeycodeConverter::DomCodeToEvdevCode(DomCode code) {
  // Currently XKB keycode is the native keycode.
  // TODO(crbug.com/40151699): Replace with evdev.
  return XkbKeycodeToEvdevCode(DomCodeToXkbKeycode(code));
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
// static
DomCode KeycodeConverter::MapUSPositionalShortcutKeyToDomCode(
    KeyboardCode key_code,
    DomCode original_dom_code) {
  if (!ShouldPositionallyRemapKey(key_code, original_dom_code)) {
    return DomCode::NONE;
  }

  // VKEY Mapping: http://kbdlayout.info/kbdus/overview+virtualkeys
  // DomCode Mapping:
  //     https://www.w3.org/TR/DOM-Level-3-Events-code/#writing-system-keys
  switch (key_code) {
    case VKEY_OEM_MINUS:
      return DomCode::MINUS;
    case VKEY_OEM_PLUS:
      return DomCode::EQUAL;
    case VKEY_OEM_2:
      return DomCode::SLASH;
    case VKEY_OEM_4:
      return DomCode::BRACKET_LEFT;
    case VKEY_OEM_6:
      return DomCode::BRACKET_RIGHT;
    case VKEY_OEM_COMMA:
      return DomCode::COMMA;
    case VKEY_OEM_PERIOD:
      return DomCode::PERIOD;
    case VKEY_OEM_1:
      return DomCode::SEMICOLON;
    case VKEY_OEM_7:
      return DomCode::QUOTE;
    case VKEY_OEM_3:
      return DomCode::BACKQUOTE;
    case VKEY_OEM_5:
      return DomCode::BACKSLASH;
    case VKEY_OEM_102:
      return DomCode::INTL_BACKSLASH;
    default:
      return DomCode::NONE;
  }
}

// static
KeyboardCode KeycodeConverter::MapPositionalDomCodeToUSShortcutKey(
    DomCode code,
    KeyboardCode original_key_code) {
  if (!ShouldPositionallyRemapKey(original_key_code, code)) {
    return VKEY_UNKNOWN;
  }

  // VKEY Mapping: http://kbdlayout.info/kbdus/overview+virtualkeys
  // DomCode Mapping:
  //     https://www.w3.org/TR/DOM-Level-3-Events-code/#writing-system-keys
  switch (code) {
    case DomCode::MINUS:
      return VKEY_OEM_MINUS;
    case DomCode::EQUAL:
      return VKEY_OEM_PLUS;
    case DomCode::SLASH:
      return VKEY_OEM_2;
    case DomCode::BRACKET_LEFT:
      return VKEY_OEM_4;
    case DomCode::BRACKET_RIGHT:
      return VKEY_OEM_6;
    case DomCode::COMMA:
      return VKEY_OEM_COMMA;
    case DomCode::PERIOD:
      return VKEY_OEM_PERIOD;
    case DomCode::SEMICOLON:
      return VKEY_OEM_1;
    case DomCode::QUOTE:
      return VKEY_OEM_7;
    case DomCode::BACKQUOTE:
      return VKEY_OEM_3;
    case DomCode::BACKSLASH:
      return VKEY_OEM_5;
    case DomCode::INTL_BACKSLASH:
      return VKEY_OEM_102;
    default:
      return VKEY_UNKNOWN;
  }
}
#endif

// static
DomCode KeycodeConverter::CodeStringToDomCode(std::string_view code) {
  if (code.empty())
    return DomCode::NONE;
  for (auto& mapping : kDomCodeMappings) {
    if (mapping.code && code == mapping.code) {
      return static_cast<DomCode>(mapping.usb_keycode);
    }
  }
  LOG(WARNING) << "unrecognized code string '" << code << "'";
  return DomCode::NONE;
}

// static
std::string KeycodeConverter::DomCodeToCodeString(DomCode dom_code) {
  const auto usb_keycode = static_cast<uint32_t>(dom_code);

  // Generate some continuous runs of codes, rather than looking them up.
  if (dom_code >= DomCode::US_A && dom_code <= DomCode::US_Z) {
    const int index = usb_keycode - static_cast<uint32_t>(DomCode::US_A);
    return base::StringPrintf("Key%c", 'A' + index);
  } else if (dom_code >= DomCode::DIGIT1 && dom_code <= DomCode::DIGIT0) {
    const int index = usb_keycode - static_cast<uint32_t>(DomCode::DIGIT1);
    return base::StringPrintf("Digit%d", (index + 1) % 10);
  } else if (dom_code >= DomCode::NUMPAD1 && dom_code <= DomCode::NUMPAD0) {
    const int index = usb_keycode - static_cast<uint32_t>(DomCode::NUMPAD1);
    return base::StringPrintf("Numpad%d", (index + 1) % 10);
  } else if (dom_code >= DomCode::F1 && dom_code <= DomCode::F12) {
    const int index = usb_keycode - static_cast<uint32_t>(DomCode::F1);
    return base::StringPrintf("F%d", index + 1);
  } else if (dom_code >= DomCode::F13 && dom_code <= DomCode::F24) {
    const int index = usb_keycode - static_cast<uint32_t>(DomCode::F13);
    return base::StringPrintf("F%d", index + 13);
  }

  for (auto& mapping : kDomCodeMappings) {
    if (mapping.usb_keycode == usb_keycode) {
      if (mapping.code)
        return mapping.code;
      break;
    }
  }
  return "";
}

// static
DomKeyLocation KeycodeConverter::DomCodeToLocation(DomCode dom_code) {
  static const struct {
    DomCode code;
    DomKeyLocation location;
  } kLocations[] = {{DomCode::CONTROL_LEFT, DomKeyLocation::LEFT},
                    {DomCode::SHIFT_LEFT, DomKeyLocation::LEFT},
                    {DomCode::ALT_LEFT, DomKeyLocation::LEFT},
                    {DomCode::META_LEFT, DomKeyLocation::LEFT},
                    {DomCode::CONTROL_RIGHT, DomKeyLocation::RIGHT},
                    {DomCode::SHIFT_RIGHT, DomKeyLocation::RIGHT},
                    {DomCode::ALT_RIGHT, DomKeyLocation::RIGHT},
                    {DomCode::META_RIGHT, DomKeyLocation::RIGHT},
                    {DomCode::NUMPAD_DIVIDE, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_MULTIPLY, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_SUBTRACT, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_ADD, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_ENTER, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD1, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD2, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD3, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD4, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD5, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD6, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD7, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD8, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD9, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD0, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_DECIMAL, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_EQUAL, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_COMMA, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_PAREN_LEFT, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_PAREN_RIGHT, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_BACKSPACE, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_MEMORY_STORE, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_MEMORY_RECALL, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_MEMORY_CLEAR, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_MEMORY_ADD, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_MEMORY_SUBTRACT, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_SIGN_CHANGE, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_CLEAR, DomKeyLocation::NUMPAD},
                    {DomCode::NUMPAD_CLEAR_ENTRY, DomKeyLocation::NUMPAD}};
  for (const auto& key : kLocations) {
    if (key.code == dom_code)
      return key.location;
  }
  return DomKeyLocation::STANDARD;
}

// static
DomKey KeycodeConverter::KeyStringToDomKey(std::string_view key) {
  if (key.empty())
    return DomKey::NONE;
  // Check for standard key names.
  for (auto& mapping : kDomKeyMappings) {
    if (mapping.string && key == mapping.string) {
      return mapping.dom_key;
    }
  }
  if (key == "Dead") {
    // The web KeyboardEvent string does not encode the combining character,
    // so we just set it to the Unicode designated non-character 0xFFFF.
    // This will round-trip convert back to 'Dead' but take no part in
    // character composition.
    return DomKey::DeadKeyFromCombiningCharacter(0xFFFF);
  }
  // Otherwise, if the string contains a single Unicode character,
  // the key value is that character.
  const size_t key_length = key.length();
  size_t char_index = 0;
  base_icu::UChar32 character;
  if (base::ReadUnicodeCharacter(key.data(), key_length, &char_index,
                                 &character) &&
      ++char_index == key_length) {
    return DomKey::FromCharacter(character);
  }
  return DomKey::NONE;
}

// static
std::string KeycodeConverter::DomKeyToKeyString(DomKey dom_key) {
  if (dom_key.IsDeadKey()) {
    // All dead-key combining codes collapse to 'Dead', as UI Events
    // KeyboardEvent represents the combining character separately.
    return "Dead";
  }
  for (auto& mapping : kDomKeyMappings) {
    if (mapping.dom_key == dom_key) {
      if (mapping.string)
        return mapping.string;
      break;
    }
  }
  if (dom_key.IsCharacter()) {
    std::string s;
    base::WriteUnicodeCharacter(dom_key.ToCharacter(), &s);
    return s;
  }
  return std::string();
}

// static
bool KeycodeConverter::IsDomKeyForModifier(DomKey dom_key) {
  switch (dom_key) {
    case DomKey::ACCEL:
    case DomKey::ALT:
    case DomKey::ALT_GRAPH:
    case DomKey::CAPS_LOCK:
    case DomKey::CONTROL:
    case DomKey::FN:
    case DomKey::FN_LOCK:
    case DomKey::HYPER:
    case DomKey::META:
    case DomKey::NUM_LOCK:
    case DomKey::SCROLL_LOCK:
    case DomKey::SHIFT:
    case DomKey::SUPER:
    case DomKey::SYMBOL:
    case DomKey::SYMBOL_LOCK:
    case DomKey::SHIFT_LEVEL5:
      return true;
    default:
      return false;
  }
}

// static
bool KeycodeConverter::IsDomKeyNamed(DomKey dom_key) {
  if (dom_key.IsDeadKey()) {
    return true;
  }
  for (auto& mapping : kDomKeyMappings) {
    if (mapping.dom_key == dom_key) {
      return mapping.string != nullptr;
    }
  }
  return false;
}

// USB keycodes
// Note that USB keycodes are not part of any web standard.
// Please don't use USB keycodes in new code.

// static
uint32_t KeycodeConverter::InvalidUsbKeycode() {
  return kDomCodeMappings[0].usb_keycode;
}

// static
int KeycodeConverter::UsbKeycodeToNativeKeycode(uint32_t usb_keycode) {
  // Deal with some special-cases that don't fit the 1:1 mapping.
  if (usb_keycode == 0x070032)  // non-US hash.
    usb_keycode = 0x070031;     // US backslash.
#if BUILDFLAG(IS_APPLE)
  if (usb_keycode == 0x070046) // PrintScreen.
    usb_keycode = 0x070068; // F13.
#endif

  for (auto& mapping : kDomCodeMappings) {
    if (mapping.usb_keycode == usb_keycode)
      return mapping.native_keycode;
  }
  return InvalidNativeKeycode();
}

// static
uint32_t KeycodeConverter::NativeKeycodeToUsbKeycode(int native_keycode) {
  for (auto& mapping : kDomCodeMappings) {
    if (mapping.native_keycode == native_keycode)
      return mapping.usb_keycode;
  }
  return InvalidUsbKeycode();
}

// static
DomCode KeycodeConverter::UsbKeycodeToDomCode(uint32_t usb_keycode) {
  for (auto& mapping : kDomCodeMappings) {
    if (mapping.usb_keycode == usb_keycode)
      return static_cast<DomCode>(usb_keycode);
  }
  return DomCode::NONE;
}

// static
uint32_t KeycodeConverter::DomCodeToUsbKeycode(DomCode dom_code) {
  for (auto& mapping : kDomCodeMappings) {
    if (mapping.usb_keycode == static_cast<uint32_t>(dom_code))
      return mapping.usb_keycode;
  }
  return InvalidUsbKeycode();
}

}  // namespace ui
