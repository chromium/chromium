// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_minimal.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/events/event.h"

namespace ui {
namespace {

class InputMethodDelegateForTesting : public internal::InputMethodDelegate {
 public:
  InputMethodDelegateForTesting(bool propagation)
      : propagation_post_ime_(propagation) {}
  ~InputMethodDelegateForTesting() override {}

  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* key_event) override {
    if (!propagation_post_ime_)
      key_event->StopPropagation();
    return ui::EventDispatchDetails();
  }

 private:
  bool propagation_post_ime_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodDelegateForTesting);
};

class InputMethodMinimalTest : public testing::Test {
 protected:
  InputMethodMinimalTest() = default;
  ~InputMethodMinimalTest() override = default;

  void SetUp() override {
    delegate_ = std::make_unique<InputMethodDelegateForTesting>(true);
    input_method_minimal_ =
        std::make_unique<InputMethodMinimal>(delegate_.get());
    input_method_minimal_->OnFocus();
  }

  std::unique_ptr<InputMethodMinimal> input_method_minimal_;
  std::unique_ptr<InputMethodDelegateForTesting> delegate_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMinimalTest);
};

TEST_F(InputMethodMinimalTest, StopPropagationTest) {
  std::unique_ptr<DummyTextInputClient> client =
      std::make_unique<DummyTextInputClient>();
  input_method_minimal_->SetFocusedTextInputClient(client.get());
  input_method_minimal_->OnTextInputTypeChanged(client.get());

  KeyEvent key(ET_KEY_PRESSED, VKEY_TAB, 0);
  input_method_minimal_->DispatchKeyEvent(&key);

  EXPECT_EQ(1, client->insert_char_count());
  EXPECT_EQ(9, client->last_insert_char());

  KeyEvent key_a(ET_KEY_PRESSED, VKEY_A, 0);
  input_method_minimal_->DispatchKeyEvent(&key_a);

  EXPECT_EQ(2, client->insert_char_count());
  EXPECT_EQ(97, client->last_insert_char());

  std::unique_ptr<InputMethodDelegateForTesting> delegate_no_propagation =
      std::make_unique<InputMethodDelegateForTesting>(false);
  input_method_minimal_->SetDelegate(delegate_no_propagation.get());
  input_method_minimal_->DispatchKeyEvent(&key);

  EXPECT_EQ(2, client->insert_char_count());
  EXPECT_EQ(97, client->last_insert_char());
}

}  // namespace
}  // namespace ui
