// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/linux/xdg_shortcut.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

TEST(AcceleratorToXdgShortcutTest, BasicKeys) {
  // Test alphabet keys
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_A, 0)), "a");
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_Z, 0)), "z");

  // Test number keys
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_0, 0)), "0");
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_9, 0)), "9");

  // Test general keys
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_SPACE, 0)), "space");
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_HOME, 0)), "Home");

  // Test arrow keys
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_LEFT, 0)), "Left");
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_UP, 0)), "Up");

  // Test media keys
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_MEDIA_PLAY_PAUSE, 0)),
            "XF86AudioPlay");
}

TEST(AcceleratorToXdgShortcutTest, Modifiers) {
  // Test CTRL modifier
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_A, EF_CONTROL_DOWN)),
            "CTRL+a");

  // Test SHIFT modifier
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_A, EF_SHIFT_DOWN)),
            "SHIFT+a");

  // Test ALT modifier
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_A, EF_ALT_DOWN)),
            "ALT+a");

  // Test LOGO (CMD) modifier
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_A, EF_COMMAND_DOWN)),
            "LOGO+a");

  // Test multiple modifiers
  EXPECT_EQ(AcceleratorToXdgShortcut(
                Accelerator(VKEY_A, EF_CONTROL_DOWN | EF_SHIFT_DOWN)),
            "CTRL+SHIFT+a");

  EXPECT_EQ(AcceleratorToXdgShortcut(
                Accelerator(VKEY_A, EF_CONTROL_DOWN | EF_ALT_DOWN)),
            "CTRL+ALT+a");

  EXPECT_EQ(AcceleratorToXdgShortcut(
                Accelerator(VKEY_A, EF_CONTROL_DOWN | EF_ALT_DOWN |
                                        EF_SHIFT_DOWN | EF_COMMAND_DOWN)),
            "CTRL+ALT+SHIFT+LOGO+a");
}

}  // namespace ui
