// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mac/input_method_mac.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/dummy_text_input_client.h"

namespace ui {
namespace {

class TestInputMethodMac : public InputMethodMac {
 public:
  TestInputMethodMac() : InputMethodMac(nullptr) {}

  void CancelComposition(const TextInputClient* client) override {
    EXPECT_EQ(client, GetTextInputClient());
    ++cancel_composition_count_;
  }

  int cancel_composition_count() const { return cancel_composition_count_; }

 private:
  int cancel_composition_count_ = 0;
};

TEST(InputMethodMacTest, CancelsCompositionBeforeChangingFocusedClient) {
  TestInputMethodMac input_method;
  DummyTextInputClient first_client;
  DummyTextInputClient second_client;

  input_method.SetFocusedTextInputClient(&first_client);
  EXPECT_EQ(0, input_method.cancel_composition_count());

  input_method.SetFocusedTextInputClient(&second_client);
  EXPECT_EQ(1, input_method.cancel_composition_count());

  input_method.DetachTextInputClient(&second_client);
  EXPECT_EQ(2, input_method.cancel_composition_count());
}

}  // namespace
}  // namespace ui
