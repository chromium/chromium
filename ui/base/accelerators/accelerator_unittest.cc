// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

namespace ui {

TEST(AcceleratorTest, Repeat) {
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  EXPECT_FALSE(accelerator_a.IsRepeat());

  const Accelerator accelerator_b(VKEY_B, EF_IS_REPEAT);
  EXPECT_TRUE(accelerator_b.IsRepeat());

  const Accelerator accelerator_b_copy(accelerator_b);
  EXPECT_TRUE(accelerator_b_copy.IsRepeat());
}

TEST(AcceleratorTest, TimeStamp) {
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  EXPECT_EQ(base::TimeTicks(), accelerator_a.time_stamp());

  const base::TimeTicks event_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1);
  KeyEvent keyevent(ET_KEY_PRESSED, VKEY_SPACE, EF_NONE, event_time);

  const Accelerator accelerator_b(keyevent);
  EXPECT_EQ(event_time, accelerator_b.time_stamp());
}

// Crash on Android builders. https://crbug.com/980267
#if defined(OS_ANDROID)
#define MAYBE_GetShortcutText DISABLED_GetShortcutText
#else
#define MAYBE_GetShortcutText GetShortcutText
#endif
TEST(AcceleratorTest, MAYBE_GetShortcutText) {
  struct {
    KeyboardCode code;
    int modifiers;
    const char* expected_long;
    const char* expected_short;
  } keys[] = {
    {VKEY_Q, EF_CONTROL_DOWN | EF_SHIFT_DOWN, "Ctrl+Shift+Q", "\u2303\u21e7Q"},
    {VKEY_A, EF_ALT_DOWN | EF_SHIFT_DOWN, "Alt+Shift+A", "\u2325\u21e7A"},
    // Regression test for https://crbug.com/867732:
    {VKEY_OEM_COMMA, EF_CONTROL_DOWN, "Ctrl+Comma", "\u2303,"},
#if defined(OS_MACOSX)
    {VKEY_T, EF_COMMAND_DOWN | EF_CONTROL_DOWN, nullptr, "\u2303\u2318T"},
#endif
  };

  for (const auto& key : keys) {
    base::string16 text =
        Accelerator(key.code, key.modifiers).GetShortcutText();
#if defined(OS_MACOSX)
    EXPECT_EQ(text, base::UTF8ToUTF16(key.expected_short));
#else
    EXPECT_EQ(text, base::UTF8ToUTF16(key.expected_long));
#endif
  }
}

TEST(AcceleratorTest, ShortcutTextForUnknownKey) {
  const Accelerator accelerator(VKEY_UNKNOWN, EF_NONE);
  EXPECT_EQ(base::string16(), accelerator.GetShortcutText());
}

}  // namespace ui
