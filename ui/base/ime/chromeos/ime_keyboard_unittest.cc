// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/fake_ime_keyboard.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"

namespace chromeos {
namespace input_method {

namespace {

class ImeKeyboardTest : public testing::Test,
                        public ImeKeyboard::Observer {
 public:
  void SetUp() override {
    xkey_ = std::make_unique<FakeImeKeyboard>();
    xkey_->AddObserver(this);
    caps_changed_ = false;
  }
  void TearDown() override {
    xkey_->RemoveObserver(this);
    xkey_.reset();
  }
  void OnCapsLockChanged(bool enabled) override {
    caps_changed_ = true;
  }
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
  std::unique_ptr<ImeKeyboard> xkey_;
  bool caps_changed_;
  bool layout_changed_;
};

// Tests CheckLayoutName() function.
TEST_F(ImeKeyboardTest, TestObserver) {
  xkey_->SetCapsLockEnabled(true);
  VerifyCapsLockChanged(true);
  xkey_->SetCurrentKeyboardLayoutByName("foo");
  VerifyLayoutChanged(true);
}

}  // namespace

}  // namespace input_method
}  // namespace chromeos
