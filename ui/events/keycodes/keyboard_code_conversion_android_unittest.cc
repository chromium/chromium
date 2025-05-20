// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion_android.h"

#include <android/keycodes.h>

#include <array>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/dom_us_layout_data.h"
#include "ui/events/test/keyboard_layout.h"

namespace ui {
namespace {

struct AndroidKeyCodeToKeyboardCode {
  int android_key_code;
  KeyboardCode keyboard_code;
};

constexpr std::array<AndroidKeyCodeToKeyboardCode, 21>
    kAndroidKeyCodeToKeyboardCodeMap = {
        {// Spot-check several key codes
         {AKEYCODE_DEL, KeyboardCode::VKEY_BACK},
         {AKEYCODE_SHIFT_LEFT, KeyboardCode::VKEY_LSHIFT},
         {AKEYCODE_BACK, KeyboardCode::VKEY_BROWSER_BACK},
         {AKEYCODE_DPAD_LEFT, KeyboardCode::VKEY_LEFT},
         {AKEYCODE_0, KeyboardCode::VKEY_0},
         {AKEYCODE_Z, KeyboardCode::VKEY_Z},
         {AKEYCODE_VOLUME_DOWN, KeyboardCode::VKEY_VOLUME_DOWN},
         {AKEYCODE_SEMICOLON, KeyboardCode::VKEY_OEM_1},
         {AKEYCODE_SLASH, KeyboardCode::VKEY_OEM_2},
         {AKEYCODE_INSERT, KeyboardCode::VKEY_INSERT},
         {AKEYCODE_F5, KeyboardCode::VKEY_F5},
         {AKEYCODE_NUMPAD_0, KeyboardCode::VKEY_NUMPAD0},
         {AKEYCODE_NUMPAD_DOT, KeyboardCode::VKEY_DECIMAL},
         {AKEYCODE_CHANNEL_DOWN, KeyboardCode::VKEY_NEXT},
         // Android keycodes mapped to the same key code.
         {AKEYCODE_DPAD_CENTER, KeyboardCode::VKEY_RETURN},
         {AKEYCODE_ENTER, KeyboardCode::VKEY_RETURN},
         {AKEYCODE_MUTE, KeyboardCode::VKEY_VOLUME_MUTE},
         {AKEYCODE_VOLUME_MUTE, KeyboardCode::VKEY_VOLUME_MUTE},
         {AKEYCODE_MEDIA_PLAY, KeyboardCode::VKEY_MEDIA_PLAY_PAUSE},
         {AKEYCODE_MEDIA_PLAY_PAUSE, KeyboardCode::VKEY_MEDIA_PLAY_PAUSE},
         // Unknown
         {AKEYCODE_UNKNOWN, KeyboardCode::VKEY_UNKNOWN}}};

}  // namespace

TEST(KeyboardCodeConversionAndroidTest, AndroidToChrome) {
  for (const auto& entry : kAndroidKeyCodeToKeyboardCodeMap) {
    EXPECT_EQ(entry.keyboard_code,
              KeyboardCodeFromAndroidKeyCode(entry.android_key_code));
  }
}

}  // namespace ui
