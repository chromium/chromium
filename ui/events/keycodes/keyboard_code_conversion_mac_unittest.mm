// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

#include <Carbon/Carbon.h>

#include "base/apple/scoped_cftyperef.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

base::apple::ScopedCFTypeRef<TISInputSourceRef> GetKeyboardLayoutById(
    CFStringRef input_source_id) {
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> filter(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 1,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionaryAddValue(filter.get(), kTISPropertyInputSourceID,
                       input_source_id);
  base::apple::ScopedCFTypeRef<CFArrayRef> list(
      TISCreateInputSourceList(filter.get(), /*includeAllInstalled=*/true));
  if (!list || CFArrayGetCount(list.get()) != 1) {
    return base::apple::ScopedCFTypeRef<TISInputSourceRef>();
  }
  return base::apple::ScopedCFTypeRef<TISInputSourceRef>(
      (TISInputSourceRef)CFArrayGetValueAtIndex(list.get(), 0),
      base::scoped_policy::RETAIN);
}

}  // namespace

TEST(KeyboardCodeConversionMac, DvorakLayoutTranslation) {
  // Query the Dvorak layout resource directly without calling
  // TISSelectInputSource(), which would mutate the global macOS login-session
  // keyboard layout across all parallel test processes.
  base::apple::ScopedCFTypeRef<TISInputSourceRef> dvorak_layout =
      GetKeyboardLayoutById(CFSTR("com.apple.keylayout.Dvorak"));
  ASSERT_TRUE(dvorak_layout);

  // Physical key 'G' (kVK_ANSI_G, 0x05) in Dvorak produces 'i'.
  auto [translated_char, is_dead_key] =
      ui::NsKeyCodeAndModifiersToCharacter(kVK_ANSI_G, 0, dvorak_layout.get());
  EXPECT_EQ('i', translated_char);
  EXPECT_FALSE(is_dead_key);

  // Physical key 'U' (kVK_ANSI_U, 0x20) in Dvorak produces 'g'.
  auto [translated_char2, is_dead_key2] =
      ui::NsKeyCodeAndModifiersToCharacter(kVK_ANSI_U, 0, dvorak_layout.get());
  EXPECT_EQ('g', translated_char2);
  EXPECT_FALSE(is_dead_key2);

  // Test with shift modifier (physical 'G' -> 'I').
  auto [translated_char_shift, is_dead_key_shift] =
      ui::NsKeyCodeAndModifiersToCharacter(kVK_ANSI_G, NSEventModifierFlagShift,
                                           dvorak_layout.get());
  EXPECT_EQ('I', translated_char_shift);
  EXPECT_FALSE(is_dead_key_shift);

  // Test with control modifier (should ignore control and return base character
  // 'i').
  auto [translated_char_ctrl, is_dead_key_ctrl] =
      ui::NsKeyCodeAndModifiersToCharacter(
          kVK_ANSI_G, NSEventModifierFlagControl, dvorak_layout.get());
  EXPECT_EQ('i', translated_char_ctrl);
  EXPECT_FALSE(is_dead_key_ctrl);
}

}  // namespace ui
