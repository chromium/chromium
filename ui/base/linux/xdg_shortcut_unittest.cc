// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/linux/xdg_shortcut.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

TEST(AcceleratorToXdgShortcutTest, BasicKeys) {
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_A, EF_NONE)), "a");
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_Z, EF_NONE)), "z");

  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_0, EF_NONE)), "0");
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_9, EF_NONE)), "9");

  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_F1, EF_NONE)), "F1");
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_F12, EF_NONE)), "F12");

  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_SPACE, EF_NONE)),
            "space");
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_HOME, EF_NONE)), "Home");

  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_LEFT, EF_NONE)), "Left");
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_UP, EF_NONE)), "Up");

  EXPECT_EQ(
      AcceleratorToXdgShortcut(Accelerator(VKEY_MEDIA_PLAY_PAUSE, EF_NONE)),
      "XF86AudioPlay");
}

TEST(AcceleratorToXdgShortcutTest, Modifiers) {
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_A, EF_CONTROL_DOWN)),
            "CTRL+a");
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_A, EF_SHIFT_DOWN)),
            "SHIFT+a");
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_A, EF_ALT_DOWN)),
            "ALT+a");
  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_A, EF_COMMAND_DOWN)),
            "LOGO+a");

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

  EXPECT_EQ(AcceleratorToXdgShortcut(Accelerator(VKEY_BACK, EF_SHIFT_DOWN)),
            "");
}

}  // namespace ui
