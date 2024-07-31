// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/keycodes/dom/keycode_converter.h"

#include <stddef.h>
#include <stdint.h>

#include <iomanip>
#include <map>
#include <set>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"

using ui::KeycodeConverter;

namespace {

// Number of native codes expected to be mapped for each kind of native code.
// These are in the same order as the columns in dom_code_data.inc
// as reflected in the DOM_CODE() macro below.
const size_t expected_mapped_key_count[] = {
    223,  // evdev
    223,  // xkb
    157,  // windows
    119,  // mac
};

const size_t kNativeColumns = std::size(expected_mapped_key_count);

struct KeycodeConverterData {
  uint32_t usb_keycode;
  const char* code;
  const char* id;
  int native_keycode[kNativeColumns];
};

#define DOM_CODE(usb, evdev, xkb, win, mac, code, id) \
  {                                                   \
    usb, code, #id, { evdev, xkb, win, mac }          \
  }
#define DOM_CODE_DECLARATION \
  const KeycodeConverterData kKeycodeConverterData[] =
#include "ui/events/keycodes/dom/dom_code_data.inc"
#undef DOM_CODE
#undef DOM_CODE_DECLARATION

const uint32_t kUsbNonExistentKeycode = 0xffffff;
const uint32_t kUsbUsBackslash = 0x070031;
const uint32_t kUsbNonUsHash = 0x070032;

TEST(UsbKeycodeMap, KeycodeConverterData) {
  // This test looks at all kinds of supported native codes.
  // Verify that there are no duplicate entries in the mapping.
  std::map<uint32_t, uint16_t> usb_to_native[kNativeColumns];
  std::map<uint16_t, uint32_t> native_to_usb[kNativeColumns];
  int invalid_native_keycode[kNativeColumns];
  for (size_t i = 0; i < kNativeColumns; ++i) {
    invalid_native_keycode[i] = kKeycodeConverterData[0].native_keycode[i];
  }
  for (const auto& it : kKeycodeConverterData) {
    SCOPED_TRACE(it.id);
    for (size_t i = 0; i < kNativeColumns; ++i) {
      if (it.native_keycode[i] == invalid_native_keycode[i])
        continue;
      // Verify that the USB or native codes aren't duplicated.
      EXPECT_EQ(0U, usb_to_native[i].count(it.usb_keycode))
          << " duplicate of USB code 0x" << std::hex << std::setfill('0')
          << std::setw(6) << it.usb_keycode
          << " to native 0x"
          << std::setw(4) << it.native_keycode[i]
          << " (previous was 0x"
          << std::setw(4) << usb_to_native[i][it.usb_keycode]
          << ")";
      usb_to_native[i][it.usb_keycode] = it.native_keycode[i];
      EXPECT_EQ(0U, native_to_usb[i].count(it.native_keycode[i]))
          << " duplicate of native code 0x" << std::hex << std::setfill('0')
          << std::setw(4) << it.native_keycode[i]
          << " to USB 0x"
          << std::setw(6) << it.usb_keycode
          << " (previous was 0x"
          << std::setw(6) << native_to_usb[i][it.native_keycode[i]]
          << ")";
      native_to_usb[i][it.native_keycode[i]] = it.usb_keycode;
    }
  }
  // Verify that the number of mapped keys is what we expect, i.e. we haven't
  // lost any, and if we've added some then the expectation has been updated.
  for (size_t i = 0; i < kNativeColumns; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(expected_mapped_key_count[i], usb_to_native[i].size());
  }
}

TEST(UsbKeycodeMap, EvdevXkb) {
  // XKB codes on a Linux system are 8 plus the corresponding evdev code.
  // Verify that this relationship holds in the keycode converter data.
  for (const auto& it : kKeycodeConverterData) {
    SCOPED_TRACE(it.id);
    int evdev_code = it.native_keycode[0];
    int xkb_code = it.native_keycode[1];
    if (evdev_code || xkb_code) {
      EXPECT_EQ(xkb_code, evdev_code + 8);
    }
  }
}

TEST(UsbKeycodeMap, Basic) {
  // Verify that the first element in the table is the "invalid" code.
  const ui::KeycodeMapEntry* keycode_map =
      ui::KeycodeConverter::GetKeycodeMapForTest();
  EXPECT_EQ(ui::KeycodeConverter::InvalidUsbKeycode(),
            keycode_map[0].usb_keycode);
  EXPECT_EQ(ui::KeycodeConverter::InvalidNativeKeycode(),
            keycode_map[0].native_keycode);
  EXPECT_EQ(ui::KeycodeConverter::InvalidNativeKeycode(),
            ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::NONE));

  size_t numEntries = ui::KeycodeConverter::NumKeycodeMapEntriesForTest();
  for (size_t i = 0; i < numEntries; ++i) {
    const ui::KeycodeMapEntry* entry = &keycode_map[i];
    // Don't test keys with no native keycode mapping on this platform.
    if (entry->native_keycode == ui::KeycodeConverter::InvalidNativeKeycode())
      continue;

    // Verify UsbKeycodeToNativeKeycode works for this key.
    EXPECT_EQ(
        entry->native_keycode,
        ui::KeycodeConverter::UsbKeycodeToNativeKeycode(entry->usb_keycode));

    // Verify DomCodeToNativeKeycode works correctly.
    if (entry->code && *entry->code) {
      ui::DomCode dom_code =
          ui::KeycodeConverter::CodeStringToDomCode(entry->code);
      EXPECT_EQ(entry->native_keycode,
                ui::KeycodeConverter::DomCodeToNativeKeycode(dom_code));
    }
  }
}

TEST(UsbKeycodeMap, NonExistent) {
  // Verify that UsbKeycodeToNativeKeycode works for a non-existent USB keycode.
  EXPECT_EQ(
      ui::KeycodeConverter::InvalidNativeKeycode(),
      ui::KeycodeConverter::UsbKeycodeToNativeKeycode(kUsbNonExistentKeycode));
}

TEST(UsbKeycodeMap, UsBackslashIsNonUsHash) {
  // Verify that UsbKeycodeToNativeKeycode treats the non-US "hash" key
  // as equivalent to the US "backslash" key.
  EXPECT_EQ(ui::KeycodeConverter::UsbKeycodeToNativeKeycode(kUsbUsBackslash),
            ui::KeycodeConverter::UsbKeycodeToNativeKeycode(kUsbNonUsHash));
}

TEST(KeycodeConverter, DomCode) {
  // Test invalid and unknown arguments to CodeStringToDomCode()
  EXPECT_EQ(ui::DomCode::NONE, ui::KeycodeConverter::CodeStringToDomCode("-"));
  EXPECT_EQ(ui::DomCode::NONE, ui::KeycodeConverter::CodeStringToDomCode(""));
  // Round-trip test DOM Level 3 .code strings.
  const ui::KeycodeMapEntry* keycode_map =
      ui::KeycodeConverter::GetKeycodeMapForTest();
  size_t numEntries = ui::KeycodeConverter::NumKeycodeMapEntriesForTest();
  for (size_t i = 0; i < numEntries; ++i) {
    SCOPED_TRACE(i);
    const ui::KeycodeMapEntry* entry = &keycode_map[i];
    if (entry->code) {
      ui::DomCode code = ui::KeycodeConverter::CodeStringToDomCode(entry->code);
      EXPECT_STREQ(entry->code,
                   ui::KeycodeConverter::DomCodeToCodeString(code).c_str());
    }
    ui::DomCode code =
        ui::KeycodeConverter::NativeKeycodeToDomCode(entry->native_keycode);
    EXPECT_EQ(entry->native_keycode,
              ui::KeycodeConverter::DomCodeToNativeKeycode(code));
  }
}

TEST(KeycodeConverter, DomKey) {
  const struct {
    ui::DomKey::Base key;
    bool is_character;
    bool is_dead;
    bool test_to_string;
    const char* const string;
    bool is_named;
  } test_cases[] = {
      // Invalid arguments to KeyStringToDomKey().
      {ui::DomKey::NONE, false, false, true, "", false},
      {ui::DomKey::NONE, false, false, false, "?!?", false},
      {ui::DomKey::NONE, false, false, false, "\x61\xCC\x81", false},
      // Some single Unicode characters.
      {ui::DomKey::FromCharacter('-'), true, false, true, "-", false},
      {ui::DomKey::FromCharacter('A'), true, false, true, "A", false},
      {ui::DomKey::FromCharacter(0xE1), true, false, true, "\xC3\xA1", false},
      {ui::DomKey::FromCharacter(0x1F648), true, false, true,
       "\xF0\x9F\x99\x88", false},
      // Unicode-equivalent named values.
      {ui::DomKey::BACKSPACE, true, false, true, "Backspace", true},
      {ui::DomKey::TAB, true, false, true, "Tab", true},
      {ui::DomKey::ENTER, true, false, true, "Enter", true},
      {ui::DomKey::ESCAPE, true, false, true, "Escape", true},
      {ui::DomKey::DEL, true, false, true, "Delete", true},
      {ui::DomKey::BACKSPACE, true, false, false, "\b", true},
      {ui::DomKey::TAB, true, false, false, "\t", true},
      {ui::DomKey::ENTER, true, false, false, "\r", true},
      {ui::DomKey::ESCAPE, true, false, false, "\x1B", true},
      {ui::DomKey::DEL, true, false, false, "\x7F", true},
      {ui::DomKey::FromCharacter('\b'), true, false, true, "Backspace", true},
      {ui::DomKey::FromCharacter('\t'), true, false, true, "Tab", true},
      {ui::DomKey::FromCharacter('\r'), true, false, true, "Enter", true},
      {ui::DomKey::FromCharacter(0x1B), true, false, true, "Escape", true},
      {ui::DomKey::FromCharacter(0x7F), true, false, true, "Delete", true},
      // 'Dead' key.
      {ui::DomKey::DeadKeyFromCombiningCharacter(0xFFFF), false, true, true,
       "Dead", true},
      // Sample non-Unicode key names.
      {ui::DomKey::SHIFT, false, false, true, "Shift", true},
      {ui::DomKey::F16, false, false, true, "F16", true},
      {ui::DomKey::ZOOM_IN, false, false, true, "ZoomIn", true},
      {ui::DomKey::UNIDENTIFIED, false, false, true, "Unidentified", true},
  };
  for (const auto& test : test_cases) {
    // Check KeyStringToDomKey().
    ui::DomKey key = ui::KeycodeConverter::KeyStringToDomKey(test.string);
    EXPECT_EQ(test.is_character, key.IsCharacter());
    EXPECT_EQ(test.is_dead, key.IsDeadKey());
    EXPECT_EQ(test.key, key);
    // Check |DomKeyToKeyString()|.
    if (test.test_to_string) {
      std::string s(ui::KeycodeConverter::DomKeyToKeyString(test.key));
      EXPECT_STREQ(test.string, s.c_str());
    }
    EXPECT_EQ(ui::KeycodeConverter::IsDomKeyNamed(key), test.is_named);
  }
  // Round-trip test all UI Events KeyboardEvent.key strings, and check
  // that encodings are distinct.
  std::set<ui::DomKey::Base> keys;
  const char* s = nullptr;
  for (size_t i = 0;
       (s = ui::KeycodeConverter::DomKeyStringForTest(i)) != nullptr; ++i) {
    SCOPED_TRACE(i);
    ui::DomKey key = ui::KeycodeConverter::KeyStringToDomKey(s);
    if (s) {
      EXPECT_STREQ(s, ui::KeycodeConverter::DomKeyToKeyString(key).c_str());
      if (keys.count(key) == 0) {
        keys.insert(key);
      } else {
        ADD_FAILURE() << "duplicate encoding:" << key;
      }
    } else {
      EXPECT_EQ(ui::DomKey::NONE, key);
    }
  }
}

}  // namespace
