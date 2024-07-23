// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_minimal.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/events/event.h"
#include "ui/events/test/keyboard_layout.h"

namespace ui {
namespace {

class ImeKeyEventDispatcherForTesting : public ImeKeyEventDispatcher {
 public:
  ImeKeyEventDispatcherForTesting(bool propagation)
      : propagation_post_ime_(propagation) {}

  ImeKeyEventDispatcherForTesting(const ImeKeyEventDispatcherForTesting&) =
      delete;
  ImeKeyEventDispatcherForTesting& operator=(
      const ImeKeyEventDispatcherForTesting&) = delete;

  ~ImeKeyEventDispatcherForTesting() override {}

  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* key_event) override {
    if (!propagation_post_ime_)
      key_event->StopPropagation();
    return ui::EventDispatchDetails();
  }

 private:
  bool propagation_post_ime_;
};

class InputMethodMinimalTest : public testing::Test {
 public:
  InputMethodMinimalTest(const InputMethodMinimalTest&) = delete;
  InputMethodMinimalTest& operator=(const InputMethodMinimalTest&) = delete;

 protected:
  InputMethodMinimalTest() = default;
  ~InputMethodMinimalTest() override = default;

  void SetUp() override {
    dispatcher_ = std::make_unique<ImeKeyEventDispatcherForTesting>(true);
    input_method_minimal_ =
        std::make_unique<InputMethodMinimal>(dispatcher_.get());
    input_method_minimal_->OnFocus();
  }

  std::unique_ptr<InputMethodMinimal> input_method_minimal_;
  std::unique_ptr<ImeKeyEventDispatcherForTesting> dispatcher_;
};

TEST_F(InputMethodMinimalTest, StopPropagationTest) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  std::unique_ptr<DummyTextInputClient> client =
      std::make_unique<DummyTextInputClient>();
  input_method_minimal_->SetFocusedTextInputClient(client.get());
  input_method_minimal_->OnTextInputTypeChanged(client.get());

  KeyEvent key(EventType::kKeyPressed, VKEY_TAB, 0);
  input_method_minimal_->DispatchKeyEvent(&key);

  EXPECT_EQ(1, client->insert_char_count());
  EXPECT_EQ(9, client->last_insert_char());

  KeyEvent key_a(EventType::kKeyPressed, VKEY_A, 0);
  input_method_minimal_->DispatchKeyEvent(&key_a);

  EXPECT_EQ(2, client->insert_char_count());
  EXPECT_EQ(97, client->last_insert_char());

  std::unique_ptr<ImeKeyEventDispatcherForTesting> dispatcher_no_propagation =
      std::make_unique<ImeKeyEventDispatcherForTesting>(false);
  input_method_minimal_->SetImeKeyEventDispatcher(
      dispatcher_no_propagation.get());
  input_method_minimal_->DispatchKeyEvent(&key);

  EXPECT_EQ(2, client->insert_char_count());
  EXPECT_EQ(97, client->last_insert_char());
}

}  // namespace
}  // namespace ui
