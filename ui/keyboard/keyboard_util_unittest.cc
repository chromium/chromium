// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_util.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/keyboard/test/keyboard_test_util.h"

namespace keyboard {
namespace {

class KeyboardUtilTest : public testing::Test {
 public:
  KeyboardUtilTest() {}
  ~KeyboardUtilTest() override {}

  // Sets all flags controlling whether the keyboard should be shown to
  // their disabled state.
  void DisableAllFlags() {
    ResetAllFlags();
    keyboard::SetAccessibilityKeyboardEnabled(false);
    keyboard::SetTouchKeyboardEnabled(false);
    SetEnableFlag(mojom::KeyboardEnableFlag::kPolicyDisabled);
    SetEnableFlag(mojom::KeyboardEnableFlag::kExtensionDisabled);
  }

  // Sets all flags controlling whether the keyboard should be shown to
  // their enabled flag.
  void EnableAllFlags() {
    ResetAllFlags();
    keyboard::SetAccessibilityKeyboardEnabled(true);
    keyboard::SetTouchKeyboardEnabled(true);
    SetEnableFlag(mojom::KeyboardEnableFlag::kPolicyEnabled);
    SetEnableFlag(mojom::KeyboardEnableFlag::kExtensionEnabled);
  }

  // Sets all flags controlling whether the keyboard should be shown to
  // their neutral flag.
  void ResetAllFlags() {
    keyboard::SetAccessibilityKeyboardEnabled(false);
    keyboard::SetTouchKeyboardEnabled(false);
    ClearEnableFlag(mojom::KeyboardEnableFlag::kPolicyDisabled);
    ClearEnableFlag(mojom::KeyboardEnableFlag::kExtensionDisabled);
    ClearEnableFlag(mojom::KeyboardEnableFlag::kPolicyEnabled);
    ClearEnableFlag(mojom::KeyboardEnableFlag::kExtensionEnabled);
  }

  void SetUp() override { ResetAllFlags(); }

 protected:
  void SetEnableFlag(mojom::KeyboardEnableFlag flag) {
    keyboard_controller_.SetEnableFlag(flag);
  }

  void ClearEnableFlag(mojom::KeyboardEnableFlag flag) {
    keyboard_controller_.ClearEnableFlag(flag);
  }

  // Used indirectly by keyboard utils.
  KeyboardController keyboard_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardUtilTest);
};

}  // namespace

// Tests that we respect the accessibility setting.
TEST_F(KeyboardUtilTest, AlwaysShowIfA11yEnabled) {
  // Disabled by default.
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
  // If enabled by accessibility, should ignore other flag values.
  DisableAllFlags();
  keyboard::SetAccessibilityKeyboardEnabled(true);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
}

// Tests that we respect the policy setting.
TEST_F(KeyboardUtilTest, AlwaysShowIfPolicyEnabled) {
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
  // If policy is enabled, should ignore other flag values.
  DisableAllFlags();
  SetEnableFlag(mojom::KeyboardEnableFlag::kPolicyEnabled);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
}

// Tests that we respect the policy setting.
TEST_F(KeyboardUtilTest, HidesIfPolicyDisabled) {
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
  EnableAllFlags();
  // Set accessibility to neutral since accessibility has higher precedence.
  keyboard::SetAccessibilityKeyboardEnabled(false);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  // Disable policy. Keyboard should be disabled.
  SetEnableFlag(mojom::KeyboardEnableFlag::kPolicyDisabled);
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
}

// Tests that the keyboard shows when requested flag provided higher priority
// flags have not been set.
TEST_F(KeyboardUtilTest, ShowKeyboardWhenRequested) {
  DisableAllFlags();
  // Remove device policy, which has higher precedence than us.
  ClearEnableFlag(mojom::KeyboardEnableFlag::kPolicyDisabled);
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
  // Requested should have higher precedence than all the remaining flags.
  SetEnableFlag(mojom::KeyboardEnableFlag::kExtensionEnabled);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
}

// Tests that the touch keyboard is hidden when requested flag is disabled and
// higher priority flags have not been set.
TEST_F(KeyboardUtilTest, HideKeyboardWhenRequested) {
  EnableAllFlags();
  // Remove higher precedence flags.
  ClearEnableFlag(mojom::KeyboardEnableFlag::kPolicyEnabled);
  keyboard::SetAccessibilityKeyboardEnabled(false);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  // Set requested flag to disable. Keyboard should disable.
  SetEnableFlag(mojom::KeyboardEnableFlag::kExtensionDisabled);
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
}

// SetTouchKeyboardEnabled has the lowest priority, but should still work when
// none of the other flags are enabled.
TEST_F(KeyboardUtilTest, HideKeyboardWhenTouchEnabled) {
  ResetAllFlags();
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
  keyboard::SetTouchKeyboardEnabled(true);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
}

TEST_F(KeyboardUtilTest, UpdateKeyboardConfig) {
  ResetAllFlags();
  mojom::KeyboardConfig config = keyboard_controller_.keyboard_config();
  EXPECT_TRUE(config.spell_check);
  EXPECT_FALSE(keyboard_controller_.UpdateKeyboardConfig(config));

  config.spell_check = false;
  EXPECT_TRUE(keyboard_controller_.UpdateKeyboardConfig(config));
  EXPECT_FALSE(keyboard_controller_.keyboard_config().spell_check);

  EXPECT_FALSE(keyboard_controller_.UpdateKeyboardConfig(config));
}

TEST_F(KeyboardUtilTest, IsOverscrollEnabled) {
  ResetAllFlags();

  // Return false when keyboard is disabled.
  EXPECT_FALSE(keyboard_controller_.IsKeyboardOverscrollEnabled());

  // Enable the virtual keyboard.
  keyboard::SetTouchKeyboardEnabled(true);
  EXPECT_TRUE(keyboard_controller_.IsKeyboardOverscrollEnabled());

  // Set overscroll enabled flag.
  mojom::KeyboardConfig config = keyboard_controller_.keyboard_config();
  config.overscroll_behavior =
      keyboard::mojom::KeyboardOverscrollBehavior::kDisabled;
  keyboard_controller_.UpdateKeyboardConfig(config);
  EXPECT_FALSE(keyboard_controller_.IsKeyboardOverscrollEnabled());

  config.overscroll_behavior =
      keyboard::mojom::KeyboardOverscrollBehavior::kDefault;
  keyboard_controller_.UpdateKeyboardConfig(config);
  EXPECT_TRUE(keyboard_controller_.IsKeyboardOverscrollEnabled());

  // Set keyboard_locked() to true.
  ui::DummyInputMethod input_method;
  keyboard_controller_.EnableKeyboard(
      std::make_unique<TestKeyboardUI>(&input_method), nullptr);
  keyboard_controller_.set_keyboard_locked(true);
  EXPECT_TRUE(keyboard_controller_.keyboard_locked());
  EXPECT_FALSE(keyboard_controller_.IsKeyboardOverscrollEnabled());
  keyboard_controller_.DisableKeyboard();
}

}  // namespace keyboard
