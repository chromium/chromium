// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/dom/keycode_converter.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "build/build_config.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include <linux/input.h>
#endif

namespace ui {

namespace {

// Table of USB codes (equivalent to DomCode values), native scan codes,
// and DOM Level 3 |code| strings.
#if defined(OS_WIN)
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  { usb, win, code }
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  { usb, xkb, code }
#elif defined(OS_APPLE)
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  { usb, mac, code }
#elif defined(OS_ANDROID)
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  { usb, evdev, code }
#elif defined(OS_FUCHSIA)
// TODO(https://crbug.com/1107418): Fuchsia currently delivers events
// with a USB Code but no Page specified, so only map |native_keycode| for
// Keyboard Usage Page codes, for now.
inline constexpr uint32_t CodeIfOnKeyboardPage(uint32_t usage) {
  constexpr uint32_t kUsbHidKeyboardPageBase = 0x070000;
  if ((usage & 0xffff0000) == kUsbHidKeyboardPageBase)
    return usage & 0xffff;
  return 0;
}
#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  { usb, CodeIfOnKeyboardPage(usb), code }
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

#define DOM_KEY_MAP_DECLARATION constexpr DomKeyMapEntry kDomKeyMappings[] =
#define DOM_KEY_UNI(key, id, value) {DomKey::id, key}
#define DOM_KEY_MAP(key, id, value) {DomKey::id, key}
#include "ui/events/keycodes/dom/dom_key_data.inc"
#undef DOM_KEY_MAP_DECLARATION
#undef DOM_KEY_MAP
#undef DOM_KEY_UNI

#if defined(OS_LINUX) || defined(OS_CHROMEOS)

// The offset between XKB Keycode and evdev code.
constexpr int kXkbKeycodeOffset = 8;

// TODO(crbug.com/1135034): After migrating native code for
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

  // TODO(crbug.com/1135034): Move this to EvdevCodeToDomCode on
  // migration.
  if (evdev_code == KEY_PLAYCD)
    evdev_code = KEY_PLAY;
  return static_cast<uint32_t>(evdev_code + kXkbKeycodeOffset);
}

#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

}  // namespace

// static
size_t KeycodeConverter::NumKeycodeMapEntriesForTest() {
  return base::size(kDomCodeMappings);
}

// static
const KeycodeMapEntry* KeycodeConverter::GetKeycodeMapForTest() {
  return &kDomCodeMappings[0];
}

// static
const char* KeycodeConverter::DomKeyStringForTest(size_t index) {
  if (index >= base::size(kDomKeyMappings))
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

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
// static
DomCode KeycodeConverter::XkbKeycodeToDomCode(uint32_t xkb_keycode) {
  // Currently XKB keycode is the native keycode.
  // TODO(crbug.com/1135034): Replace with evdev.
  return NativeKeycodeToDomCode(static_cast<int>(xkb_keycode));
}

// static
uint32_t KeycodeConverter::DomCodeToXkbKeycode(DomCode code) {
  // Currently XKB keycode is the native keycode.
  // TODO(crbug.com/1135034): Replace with evdev.
  return static_cast<uint32_t>(DomCodeToNativeKeycode(code));
}

// static
DomCode KeycodeConverter::EvdevCodeToDomCode(int evdev_code) {
  // Currently XKB keycode is the native keycode.
  // TODO(crbug.com/1135034): Replace with evdev.
  return XkbKeycodeToDomCode(EvdevCodeToXkbKeycode(evdev_code));
}

// static
int KeycodeConverter::DomCodeToEvdevCode(DomCode code) {
  // Currently XKB keycode is the native keycode.
  // TODO(crbug.com/1135034): Replace with evdev.
  return XkbKeycodeToEvdevCode(DomCodeToXkbKeycode(code));
}
#endif

// static
DomCode KeycodeConverter::CodeStringToDomCode(const std::string& code) {
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
const char* KeycodeConverter::DomCodeToCodeString(DomCode dom_code) {
  for (auto& mapping : kDomCodeMappings) {
    if (mapping.usb_keycode == static_cast<uint32_t>(dom_code)) {
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
DomKey KeycodeConverter::KeyStringToDomKey(const std::string& key) {
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
  int32_t char_index = 0;
  uint32_t character;
  if (base::ReadUnicodeCharacter(key.c_str(),
                                 static_cast<int32_t>(key.length()),
                                 &char_index, &character) &&
      key[++char_index] == 0) {
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
#if defined(OS_APPLE)
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

// static
uint32_t KeycodeConverter::CodeStringToUsbKeycode(const std::string& code) {
  if (code.empty())
    return InvalidUsbKeycode();

  for (auto& mapping : kDomCodeMappings) {
    if (mapping.code && code == mapping.code) {
      return mapping.usb_keycode;
    }
  }
  return InvalidUsbKeycode();
}

// static
int KeycodeConverter::CodeStringToNativeKeycode(const std::string& code) {
  return UsbKeycodeToNativeKeycode(CodeStringToUsbKeycode(code));
}

}  // namespace ui
