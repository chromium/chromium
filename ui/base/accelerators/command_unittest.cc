// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/command.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

struct CommandTestParams {
  std::string expected_hotkey_string;
  ui::Accelerator expected_hotkey_accelerator;
};

class CommandTest : public testing::Test,
                    public testing::WithParamInterface<CommandTestParams> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    CommandTest,
    ::testing::ValuesIn(std::vector<CommandTestParams>{
        {"Ctrl+A", ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN)},
        {"Ctrl+Shift+A",
         ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)},
        {"Alt+A", ui::Accelerator(ui::VKEY_A, ui::EF_ALT_DOWN)},
        {"Ctrl+Alt+A",
         ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)},
        {"Ctrl+Alt+Shift+A",
         ui::Accelerator(ui::VKEY_A,
                         ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
                             ui::EF_SHIFT_DOWN)},
#if BUILDFLAG(IS_MAC)
        {"Command+A", ui::Accelerator(ui::VKEY_A, ui::EF_COMMAND_DOWN)},
        {"Alt+Command+A",
         ui::Accelerator(ui::VKEY_A, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN)},
        {"Ctrl+Command+A",
         ui::Accelerator(ui::VKEY_A,
                         ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN)},
#endif
    }));

#if !BUILDFLAG(IS_ANDROID)
TEST_P(CommandTest, StringToAccelerator) {
  EXPECT_EQ(
      GetParam().expected_hotkey_accelerator,
      ui::Command::StringToAccelerator(GetParam().expected_hotkey_string));
}
#endif

TEST_P(CommandTest, AcceleratorToString) {
  EXPECT_EQ(
      GetParam().expected_hotkey_string,
      ui::Command::AcceleratorToString(GetParam().expected_hotkey_accelerator));
}

TEST_F(CommandTest, InvalidShortcutStrings) {
  // No valid keys
  EXPECT_TRUE(ui::Command::StringToAccelerator("Ctrl").IsEmpty());
  EXPECT_TRUE(ui::Command::StringToAccelerator("Alt").IsEmpty());
  EXPECT_TRUE(ui::Command::StringToAccelerator("Ctrl+Shift").IsEmpty());
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(ui::Command::StringToAccelerator("Command").IsEmpty());
#endif

  // No valid modifiers
  EXPECT_TRUE(ui::Command::StringToAccelerator("A").IsEmpty());
  EXPECT_TRUE(ui::Command::StringToAccelerator("A+Shift").IsEmpty());
}

TEST_F(CommandTest, InvalidAccelerator) {
  // No valid keys
  EXPECT_TRUE(ui::Command::AcceleratorToString(
                  ui::Accelerator(ui::VKEY_UNKNOWN, ui::EF_CONTROL_DOWN))
                  .empty());
  EXPECT_TRUE(ui::Command::AcceleratorToString(
                  ui::Accelerator(ui::VKEY_UNKNOWN, ui::EF_ALT_DOWN))
                  .empty());
  EXPECT_TRUE(ui::Command::AcceleratorToString(
                  ui::Accelerator(ui::VKEY_UNKNOWN,
                                  ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN))
                  .empty());
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(ui::Command::AcceleratorToString(
                  ui::Accelerator(ui::VKEY_UNKNOWN, ui::EF_COMMAND_DOWN))
                  .empty());
#endif

  // No valid modifiers
  EXPECT_TRUE(
      ui::Command::AcceleratorToString(ui::Accelerator(ui::VKEY_A, ui::EF_NONE))
          .empty());
  EXPECT_TRUE(ui::Command::AcceleratorToString(
                  ui::Accelerator(ui::VKEY_A, ui::EF_SHIFT_DOWN))
                  .empty());
}
