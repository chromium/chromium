// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

#include <Carbon/Carbon.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/keyboard_layout.h"

namespace ui {

TEST(KeyboardCodeConversionMac, DvorakLayoutTranslation) {
  // Switch to Dvorak layout.
  ui::ScopedKeyboardLayout scoped_layout(ui::KEYBOARD_LAYOUT_DVORAK);

  // Verify that the layout switched successfully.
  base::apple::ScopedCFTypeRef<TISInputSourceRef> current_layout(
      TISCopyCurrentKeyboardInputSource());
  if (current_layout) {
    CFStringRef layout_id = (CFStringRef)TISGetInputSourceProperty(
        current_layout.get(), kTISPropertyInputSourceID);
    if (std::string layout_str = base::SysCFStringRefToUTF8(layout_id);
        layout_str != "com.apple.keylayout.Dvorak") {
      GTEST_SKIP() << "Failed to switch to Dvorak layout (current is "
                   << layout_str << ")";
    }
  } else {
    GTEST_SKIP() << "Failed to get current keyboard layout";
  }

  // Physical key 'G' (kVK_ANSI_G, 0x05) in Dvorak produces 'i'.
  auto [translated_char, is_dead_key] =
      ui::NsKeyCodeAndModifiersToCharacter(kVK_ANSI_G, 0);
  EXPECT_EQ('i', translated_char);
  EXPECT_FALSE(is_dead_key);

  // Physical key 'U' (kVK_ANSI_U, 0x20) in Dvorak produces 'g'.
  auto [translated_char2, is_dead_key2] =
      ui::NsKeyCodeAndModifiersToCharacter(kVK_ANSI_U, 0);
  EXPECT_EQ('g', translated_char2);
  EXPECT_FALSE(is_dead_key2);

  // Test with shift modifier (physical 'G' -> 'I').
  auto [translated_char_shift, is_dead_key_shift] =
      ui::NsKeyCodeAndModifiersToCharacter(kVK_ANSI_G,
                                           NSEventModifierFlagShift);
  EXPECT_EQ('I', translated_char_shift);
  EXPECT_FALSE(is_dead_key_shift);

  // Test with control modifier (should ignore control and return base character
  // 'i').
  auto [translated_char_ctrl, is_dead_key_ctrl] =
      ui::NsKeyCodeAndModifiersToCharacter(kVK_ANSI_G,
                                           NSEventModifierFlagControl);
  EXPECT_EQ('i', translated_char_ctrl);
  EXPECT_FALSE(is_dead_key_ctrl);
}

}  // namespace ui
