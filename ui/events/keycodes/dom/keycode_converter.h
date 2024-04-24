// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_DOM_KEYCODE_CONVERTER_H_
#define UI_EVENTS_KEYCODES_DOM_KEYCODE_CONVERTER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>

#include "build/build_config.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/events/keycodes/keyboard_codes_posix.h"
#endif

// For reference, the W3C UI Event spec is located at:
// http://www.w3.org/TR/uievents/

namespace ui {

enum class DomKeyLocation { STANDARD, LEFT, RIGHT, NUMPAD };

// This structure is used to define the keycode mapping table.
// It is defined here because the unittests need access to it.
typedef struct {
  // USB keycode:
  //  Upper 16-bits: USB Usage Page.
  //  Lower 16-bits: USB Usage Id: Assigned ID within this usage page.
  uint32_t usb_keycode;

  // Contains one of the following:
  //  On Linux: XKB scancode
  //  On Windows: Windows OEM scancode
  //  On Mac: Mac keycode
  //  On Fuchsia: 16-bit Code from the USB Keyboard Usage Page.
  int native_keycode;

  // The UIEvents (aka: DOM4Events) |code| value as defined in:
  // http://www.w3.org/TR/DOM-Level-3-Events-code/
  const char* code;
} KeycodeMapEntry;

// A class to convert between the current platform's native keycode (scancode)
// and platform-neutral |code| values (as defined in the W3C UI Events
// spec (http://www.w3.org/TR/uievents/).
class KeycodeConverter {
 public:
  KeycodeConverter() = delete;
  KeycodeConverter(const KeycodeConverter&) = delete;
  KeycodeConverter& operator=(const KeycodeConverter&) = delete;

  // Return the value that identifies an invalid native keycode.
  static int InvalidNativeKeycode();

  // Convert a native (Mac/Win/Linux) keycode into a DomCode.
  static DomCode NativeKeycodeToDomCode(int native_keycode);

  // Convert a DomCode into a native keycode.
  static int DomCodeToNativeKeycode(DomCode code);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Convert a XKB keycode into a DomCode.
  static DomCode XkbKeycodeToDomCode(uint32_t xkb_keycode);

  // Convert a DomCode into a XKB keycode.
  static uint32_t DomCodeToXkbKeycode(DomCode code);

  // Convert an evdev code into DomCode.
  static DomCode EvdevCodeToDomCode(int evdev_code);

  // Convert a DomCode into an evdev code.
  static int DomCodeToEvdevCode(DomCode code);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // If |key_code| is one of the keys (plus, minus, brackets, period, comma),
  // that are treated positionally for keyboard shortcuts, this returns the
  // DomCode of that key in the US layout. Any other key returns
  // |DomCode::NONE|.
  static DomCode MapUSPositionalShortcutKeyToDomCode(
      KeyboardCode key_code,
      DomCode original_dom_code = ui::DomCode::NONE);

  // If |code| is one of the keys (plus, minus, brackets, period, comma) that
  // are treated positionally for keyboard shortcuts, this returns the
  // KeyboardCode (aka VKEY) of that key in the US layout. Any other key
  // returns |VKEY_UNKNOWN|
  static KeyboardCode MapPositionalDomCodeToUSShortcutKey(
      DomCode code,
      KeyboardCode original_key_code = VKEY_UNKNOWN);
#endif

  // Conversion between DOM Code string and DomCode enum values.
  // Returns the invalid value if the supplied code is not recognized,
  // or has no mapping.
  static DomCode CodeStringToDomCode(std::string_view code);
  static std::string DomCodeToCodeString(DomCode dom_code);

  // Return the DomKeyLocation of a DomCode. The DomKeyLocation distinguishes
  // keys with the same meaning, and therefore the same DomKey or non-located
  // KeyboardCode (VKEY), and corresponds to the DOM UI Events
  // |KeyboardEvent.location|.
  static DomKeyLocation DomCodeToLocation(DomCode dom_code);

  // Convert a UI Events |key| string value into a DomKey.
  // Accepts a character string containing either
  // - a key name from http://www.w3.org/TR/DOM-Level-3-Events-key/, or
  // - a single Unicode character (represented in UTF-8).
  // Returns DomKey::NONE for other inputs, including |nullptr|.
  static DomKey KeyStringToDomKey(std::string_view key);

  // Convert a DomKey into a UI Events |key| string value.
  // Returns an empty string for invalid DomKey values.
  static std::string DomKeyToKeyString(DomKey dom_key);

  // Returns true if the DomKey is a modifier.
  static bool IsDomKeyForModifier(DomKey dom_key);

  // Returns true if the DomKey is a named key, as defined by
  // https://www.w3.org/TR/uievents-key/#named-key-attribute-value
  static bool IsDomKeyNamed(DomKey dom_key);

  // The following methods relate to USB keycodes.
  // Note that USB keycodes are not part of any web standard.
  // Please don't use USB keycodes in new code.

  // Return the value that identifies an invalid USB keycode.
  static uint32_t InvalidUsbKeycode();

  // Conversion between USB keycode and native keycode values.
  // Returns the invalid value if the supplied code is not recognized,
  // or has no mapping.
  static int UsbKeycodeToNativeKeycode(uint32_t usb_keycode);
  static uint32_t NativeKeycodeToUsbKeycode(int native_keycode);

  // Conversion between USB keycode and DomCode values.
  // Returns the "invalid" value if the supplied key code is not
  // recognized.
  static DomCode UsbKeycodeToDomCode(uint32_t usb_keycode);
  static uint32_t DomCodeToUsbKeycode(DomCode dom_code);

  // Static methods to support testing.
  static size_t NumKeycodeMapEntriesForTest();
  static const KeycodeMapEntry* GetKeycodeMapForTest();
  static const char* DomKeyStringForTest(size_t index);
};

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_DOM_KEYCODE_CONVERTER_H_
