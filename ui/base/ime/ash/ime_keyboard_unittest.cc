// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/ime_keyboard.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"

namespace ash {
namespace input_method {

namespace {

class ImeKeyboardTest : public testing::Test, public ImeKeyboard::Observer {
 public:
  void SetUp() override {
    ime_keyboard_ = std::make_unique<FakeImeKeyboard>();
    ime_keyboard_->AddObserver(this);
    caps_changed_ = false;
  }
  void TearDown() override {
    ime_keyboard_->RemoveObserver(this);
    ime_keyboard_.reset();
  }
  void OnCapsLockChanged(bool enabled) override { caps_changed_ = true; }
  void OnLayoutChanging(const std::string& layout_name) override {
    layout_changed_ = true;
  }
  void VerifyCapsLockChanged(bool changed) {
    EXPECT_EQ(changed, caps_changed_);
    caps_changed_ = false;
  }
  void VerifyLayoutChanged(bool changed) {
    EXPECT_EQ(changed, layout_changed_);
    layout_changed_ = false;
  }

  std::unique_ptr<ImeKeyboard> ime_keyboard_;
  bool caps_changed_;
  bool layout_changed_;
};

// Tests CheckLayoutName() function.
TEST_F(ImeKeyboardTest, TestObserver) {
  ime_keyboard_->SetCapsLockEnabled(true);
  VerifyCapsLockChanged(true);
  ime_keyboard_->SetCurrentKeyboardLayoutByName("foo", base::DoNothing());
  VerifyLayoutChanged(true);
}

TEST_F(ImeKeyboardTest, IsISOLevel5ShiftAvailable) {
  ime_keyboard_->SetCurrentKeyboardLayoutByName("us", base::DoNothing());
  EXPECT_FALSE(ime_keyboard_->IsISOLevel5ShiftAvailable());
  ime_keyboard_->SetCurrentKeyboardLayoutByName("ca(multix)",
                                                base::DoNothing());
  EXPECT_TRUE(ime_keyboard_->IsISOLevel5ShiftAvailable());
}

TEST_F(ImeKeyboardTest, IsAltGrAvailable) {
  ime_keyboard_->SetCurrentKeyboardLayoutByName("us", base::DoNothing());
  EXPECT_FALSE(ime_keyboard_->IsAltGrAvailable());
  ime_keyboard_->SetCurrentKeyboardLayoutByName("fr", base::DoNothing());
  EXPECT_TRUE(ime_keyboard_->IsAltGrAvailable());
}

}  // namespace

}  // namespace input_method
}  // namespace ash
