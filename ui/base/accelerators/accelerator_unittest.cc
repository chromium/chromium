// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator.h"

#include <string>

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
    const char16_t* expected_long;
    const char16_t* expected_short;
  } keys[] = {
    {VKEY_Q, EF_CONTROL_DOWN | EF_SHIFT_DOWN, u"Ctrl+Shift+Q", u"⌃⇧Q"},
    {VKEY_A, EF_ALT_DOWN | EF_SHIFT_DOWN, u"Alt+Shift+A", u"⌥⇧A"},
    // Regression test for https://crbug.com/867732:
    {VKEY_OEM_COMMA, EF_CONTROL_DOWN, u"Ctrl+Comma", u"⌃,"},
#if defined(OS_MAC)
    {VKEY_T, EF_COMMAND_DOWN | EF_CONTROL_DOWN, nullptr, u"⌃⌘T"},
#endif
  };

  for (const auto& key : keys) {
    std::u16string text =
        Accelerator(key.code, key.modifiers).GetShortcutText();
#if defined(OS_MAC)
    EXPECT_EQ(text, key.expected_short);
#else
    EXPECT_EQ(text, key.expected_long);
#endif
  }
}

TEST(AcceleratorTest, ShortcutTextForUnknownKey) {
  const Accelerator accelerator(VKEY_UNKNOWN, EF_NONE);
  EXPECT_EQ(std::u16string(), accelerator.GetShortcutText());
}

}  // namespace ui
