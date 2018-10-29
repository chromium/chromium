// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/input_method_mus.h"

#include <utility>

#include "services/ws/public/mojom/ime/ime.mojom.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/mus/input_method_mus_test_api.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method_delegate.h"

namespace aura {
namespace {

// Empty implementation of InputMethodDelegate.
class TestInputMethodDelegate : public ui::internal::InputMethodDelegate {
 public:
  TestInputMethodDelegate() {}
  ~TestInputMethodDelegate() override {}

  bool was_dispatch_key_event_post_ime_called() const {
    return was_dispatch_key_event_post_ime_called_;
  }

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* key,
      base::OnceCallback<void(bool)> ack_callback) override {
    was_dispatch_key_event_post_ime_called_ = true;
    CallDispatchKeyEventPostIMEAck(key, std::move(ack_callback));
    return ui::EventDispatchDetails();
  }

 private:
  bool was_dispatch_key_event_post_ime_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethodDelegate);
};

using ProcessKeyEventCallback = base::OnceCallback<void(bool)>;
using ProcessKeyEventCallbacks = std::vector<ProcessKeyEventCallback>;
using EventResultCallback = base::OnceCallback<void(ws::mojom::EventResult)>;

// InputMethod implementation that queues up the callbacks supplied to
// ProcessKeyEvent().
class TestInputMethod : public ws::mojom::InputMethod {
 public:
  TestInputMethod() {}
  ~TestInputMethod() override {}

  ProcessKeyEventCallbacks* process_key_event_callbacks() {
    return &process_key_event_callbacks_;
  }

  // ui::ime::InputMethod:
  void OnTextInputTypeChanged(ui::TextInputType text_input_type) override {
    was_on_text_input_type_changed_called_ = true;
  }
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override {
    was_on_caret_bounds_changed_called_ = true;
  }
  void ProcessKeyEvent(std::unique_ptr<ui::Event> key_event,
                       ProcessKeyEventCallback callback) override {
    process_key_event_callbacks_.push_back(std::move(callback));
  }
  void CancelComposition() override { was_cancel_composition_called_ = true; }
  void ShowVirtualKeyboardIfEnabled() override {
    was_show_virtual_keyboard_if_enabled_called_ = true;
  }

  bool was_on_text_input_type_changed_called() {
    return was_on_text_input_type_changed_called_;
  }

  bool was_on_caret_bounds_changed_called() {
    return was_on_caret_bounds_changed_called_;
  }

  bool was_cancel_composition_called() {
    return was_cancel_composition_called_;
  }

  bool was_show_virtual_keyboard_if_enabled_called() {
    return was_show_virtual_keyboard_if_enabled_called_;
  }

 private:
  bool was_on_text_input_type_changed_called_ = false;
  bool was_on_caret_bounds_changed_called_ = false;
  bool was_cancel_composition_called_ = false;
  bool was_show_virtual_keyboard_if_enabled_called_ = false;
  ProcessKeyEventCallbacks process_key_event_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethod);
};

}  // namespace

using InputMethodMusTest = test::AuraTestBaseMus;

namespace {

// Used in closure supplied to processing the event.
void RunFunctionWithEventResult(bool* was_run,
                                ws::mojom::EventResult* result_out,
                                ws::mojom::EventResult result) {
  *was_run = true;
  *result_out = result;
}

}  // namespace

TEST_F(InputMethodMusTest, PendingCallbackRunFromDestruction) {
  bool was_event_result_callback_run = false;
  ws::mojom::EventResult event_result = ws::mojom::EventResult::UNHANDLED;
  // Create an InputMethodMus and forward an event to it.
  {
    TestInputMethodDelegate input_method_delegate;
    InputMethodMus input_method_mus(&input_method_delegate, nullptr);
    TestInputMethod test_input_method;
    InputMethodMusTestApi::SetInputMethod(&input_method_mus,
                                          &test_input_method);
    EventResultCallback callback =
        base::BindOnce(&RunFunctionWithEventResult,
                       &was_event_result_callback_run, &event_result);

    ui::EventDispatchDetails details =
        InputMethodMusTestApi::CallSendKeyEventToInputMethod(
            &input_method_mus,
            ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0),
            std::move(callback));
    ASSERT_TRUE(!details.dispatcher_destroyed && !details.target_destroyed);

    // Add a null callback as well, to make sure null is deal with.
    details = InputMethodMusTestApi::CallSendKeyEventToInputMethod(
        &input_method_mus, ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0),
        InputMethodMus::EventResultCallback());
    ASSERT_TRUE(!details.dispatcher_destroyed && !details.target_destroyed);
    // The event should have been queued.
    EXPECT_EQ(2u, test_input_method.process_key_event_callbacks()->size());
    // Callback should not have been run yet.
    EXPECT_FALSE(was_event_result_callback_run);
  }

  // When the destructor is run the callback should be run.
  EXPECT_TRUE(was_event_result_callback_run);
}

TEST_F(InputMethodMusTest, PendingCallbackRunFromOnDidChangeFocusedClient) {
  bool was_event_result_callback_run = false;
  ws::mojom::EventResult event_result = ws::mojom::EventResult::UNHANDLED;
  ui::DummyTextInputClient test_input_client;
  // Create an InputMethodMus and forward an event to it.
  TestInputMethodDelegate input_method_delegate;
  InputMethodMus input_method_mus(&input_method_delegate, nullptr);
  TestInputMethod test_input_method;
  InputMethodMusTestApi::SetInputMethod(&input_method_mus, &test_input_method);
  EventResultCallback callback =
      base::BindOnce(&RunFunctionWithEventResult,
                     &was_event_result_callback_run, &event_result);
  ui::EventDispatchDetails details =
      InputMethodMusTestApi::CallSendKeyEventToInputMethod(
          &input_method_mus,
          ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0),
          std::move(callback));
  ASSERT_TRUE(!details.dispatcher_destroyed && !details.target_destroyed);
  // The event should have been queued.
  EXPECT_EQ(1u, test_input_method.process_key_event_callbacks()->size());
  // Callback should not have been run yet.
  EXPECT_FALSE(was_event_result_callback_run);

  InputMethodMusTestApi::CallOnDidChangeFocusedClient(
      &input_method_mus, nullptr, &test_input_client);
  // Changing the focused client should trigger running the callback.
  EXPECT_TRUE(was_event_result_callback_run);
  EXPECT_EQ(ws::mojom::EventResult::HANDLED, event_result);
}

TEST_F(InputMethodMusTest,
       PendingCallbackRunFromOnDidChangeFocusedClientToNull) {
  bool was_event_result_callback_run = false;
  ws::mojom::EventResult event_result = ws::mojom::EventResult::UNHANDLED;
  // Create an InputMethodMus and forward an event to it.
  TestInputMethodDelegate input_method_delegate;
  InputMethodMus input_method_mus(&input_method_delegate, nullptr);
  TestInputMethod test_input_method;
  InputMethodMusTestApi::SetInputMethod(&input_method_mus, &test_input_method);
  EventResultCallback callback =
      base::BindOnce(&RunFunctionWithEventResult,
                     &was_event_result_callback_run, &event_result);
  ui::EventDispatchDetails details =
      InputMethodMusTestApi::CallSendKeyEventToInputMethod(
          &input_method_mus,
          ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0),
          std::move(callback));
  ASSERT_TRUE(!details.dispatcher_destroyed && !details.target_destroyed);
  // The event should have been queued.
  EXPECT_EQ(1u, test_input_method.process_key_event_callbacks()->size());
  // Callback should not have been run yet.
  EXPECT_FALSE(was_event_result_callback_run);

  InputMethodMusTestApi::CallOnDidChangeFocusedClient(&input_method_mus,
                                                      nullptr, nullptr);
  // Changing the focused client should trigger running the callback.
  EXPECT_TRUE(was_event_result_callback_run);
  EXPECT_EQ(ws::mojom::EventResult::HANDLED, event_result);
}

// See description of ChangeTextInputTypeWhileProcessingCallback for details.
class TestInputMethodDelegate2 : public ui::internal::InputMethodDelegate {
 public:
  TestInputMethodDelegate2() {}
  ~TestInputMethodDelegate2() override {}

  void SetInputMethodAndClient(InputMethodMus* input_method_mus,
                               ui::TextInputClient* text_input_client) {
    input_method_mus_ = input_method_mus;
    text_input_client_ = text_input_client;
  }

  bool was_dispatch_key_event_post_ime_called() const {
    return was_dispatch_key_event_post_ime_called_;
  }

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* key,
      base::OnceCallback<void(bool)> ack_callback) override {
    was_dispatch_key_event_post_ime_called_ = true;
    input_method_mus_->SetFocusedTextInputClient(text_input_client_);
    CallDispatchKeyEventPostIMEAck(key, std::move(ack_callback));
    return ui::EventDispatchDetails();
  }

 private:
  InputMethodMus* input_method_mus_ = nullptr;
  ui::TextInputClient* text_input_client_ = nullptr;
  bool was_dispatch_key_event_post_ime_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethodDelegate2);
};

// This test setups the scenario where during processing an unhandled event
// SetFocusedTextInputClient() is called. This verifies we don't crash in this
// scenario and the callback is correctly called.
TEST_F(InputMethodMusTest, ChangeTextInputTypeWhileProcessingCallback) {
  bool was_event_result_callback_run = false;
  ws::mojom::EventResult event_result = ws::mojom::EventResult::UNHANDLED;
  ui::DummyTextInputClient test_input_client;
  // Create an InputMethodMus and forward an event to it.
  TestInputMethodDelegate2 input_method_delegate;
  InputMethodMus input_method_mus(&input_method_delegate, nullptr);
  input_method_delegate.SetInputMethodAndClient(&input_method_mus,
                                                &test_input_client);
  TestInputMethod test_input_method;
  InputMethodMusTestApi::SetInputMethod(&input_method_mus, &test_input_method);
  EventResultCallback callback =
      base::BindOnce(&RunFunctionWithEventResult,
                     &was_event_result_callback_run, &event_result);
  const ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0);
  ui::EventDispatchDetails details =
      InputMethodMusTestApi::CallSendKeyEventToInputMethod(
          &input_method_mus, key_event, std::move(callback));
  ASSERT_TRUE(!details.dispatcher_destroyed && !details.target_destroyed);
  // The event should have been queued.
  ASSERT_EQ(1u, test_input_method.process_key_event_callbacks()->size());
  // Callback should not have been run yet.
  EXPECT_FALSE(was_event_result_callback_run);
  const bool handled = true;
  std::move((*test_input_method.process_key_event_callbacks())[0]).Run(handled);

  // Callback should have been run.
  EXPECT_TRUE(was_event_result_callback_run);
  EXPECT_EQ(ws::mojom::EventResult::HANDLED, event_result);
  EXPECT_FALSE(input_method_delegate.was_dispatch_key_event_post_ime_called());
}

// Calling OnTextInputTypeChanged from unfocused client should
// not trigger OnTextInputTypeChanged on mus side.
TEST_F(InputMethodMusTest, ChangeTextInputTypeFromUnfocusedClient) {
  ui::DummyTextInputClient focused_input_client;
  ui::DummyTextInputClient unfocused_input_client;
  // Create an InputMethodMus and set initial text input client.
  TestInputMethodDelegate input_method_delegate;
  InputMethodMus input_method_mus(&input_method_delegate, nullptr);
  TestInputMethod test_input_method;
  InputMethodMusTestApi::SetInputMethod(&input_method_mus, &test_input_method);
  InputMethodMusTestApi::CallOnDidChangeFocusedClient(
      &input_method_mus, nullptr, &focused_input_client);

  InputMethodMusTestApi::CallOnTextInputTypeChanged(&input_method_mus,
                                                    &unfocused_input_client);

  EXPECT_FALSE(test_input_method.was_on_text_input_type_changed_called());
}

// Calling OnCaretBoundsChanged from unfocused client should
// not trigger OnCaretBoundsChanged on mus side.
TEST_F(InputMethodMusTest, ChangeCaretBoundsFromUnfocusedClient) {
  ui::DummyTextInputClient focused_input_client;
  ui::DummyTextInputClient unfocused_input_client;
  // Create an InputMethodMus and set initial text input client.
  TestInputMethodDelegate input_method_delegate;
  InputMethodMus input_method_mus(&input_method_delegate, nullptr);
  TestInputMethod test_input_method;
  InputMethodMusTestApi::SetInputMethod(&input_method_mus, &test_input_method);
  InputMethodMusTestApi::CallOnDidChangeFocusedClient(
      &input_method_mus, nullptr, &focused_input_client);

  InputMethodMusTestApi::CallOnCaretBoundsChanged(&input_method_mus,
                                                  &unfocused_input_client);

  EXPECT_FALSE(test_input_method.was_on_caret_bounds_changed_called());
}

// Calling CancelComposition from unfocused client should
// not trigger CancelComposition on mus side.
TEST_F(InputMethodMusTest, CancelCompositionFromUnfocusedClient) {
  ui::DummyTextInputClient focused_input_client;
  ui::DummyTextInputClient unfocused_input_client;
  // Create an InputMethodMus and set initial text input client.
  TestInputMethodDelegate input_method_delegate;
  InputMethodMus input_method_mus(&input_method_delegate, nullptr);
  TestInputMethod test_input_method;
  InputMethodMusTestApi::SetInputMethod(&input_method_mus, &test_input_method);
  InputMethodMusTestApi::CallOnDidChangeFocusedClient(
      &input_method_mus, nullptr, &focused_input_client);

  InputMethodMusTestApi::CallCancelComposition(&input_method_mus,
                                               &unfocused_input_client);

  EXPECT_FALSE(test_input_method.was_cancel_composition_called());
}

// Calling ShowVirtualKeyboardIfEnabled should notify the mus side.
TEST_F(InputMethodMusTest, ShowVirtualKeyboardIfEnabled) {
  TestInputMethodDelegate input_method_delegate;
  InputMethodMus input_method_mus(&input_method_delegate, nullptr);
  TestInputMethod test_input_method;
  InputMethodMusTestApi::SetInputMethod(&input_method_mus, &test_input_method);
  EXPECT_FALSE(test_input_method.was_show_virtual_keyboard_if_enabled_called());
  input_method_mus.ShowVirtualKeyboardIfEnabled();
  EXPECT_TRUE(test_input_method.was_show_virtual_keyboard_if_enabled_called());
}

TEST_F(InputMethodMusTest, AckUnhandledCallsDispatchKeyEventPostUnhandled) {
  bool was_event_result_callback_run = false;
  ws::mojom::EventResult event_result = ws::mojom::EventResult::UNHANDLED;
  // Create an InputMethodMus and forward an event to it.
  TestInputMethodDelegate input_method_delegate;
  InputMethodMus input_method_mus(&input_method_delegate, nullptr);
  TestInputMethod test_input_method;
  InputMethodMusTestApi::SetInputMethod(&input_method_mus, &test_input_method);
  EventResultCallback callback =
      base::BindOnce(&RunFunctionWithEventResult,
                     &was_event_result_callback_run, &event_result);
  const ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0);
  ignore_result(InputMethodMusTestApi::CallSendKeyEventToInputMethod(
      &input_method_mus, key_event, std::move(callback)));
  // The event should have been queued.
  ASSERT_EQ(1u, test_input_method.process_key_event_callbacks()->size());
  // Callback should not have been run yet.
  EXPECT_FALSE(was_event_result_callback_run);
  const bool handled = false;
  std::move((*test_input_method.process_key_event_callbacks())[0]).Run(handled);
  test_input_method.process_key_event_callbacks()->clear();

  // Callback should have been run.
  EXPECT_TRUE(was_event_result_callback_run);
  EXPECT_EQ(ws::mojom::EventResult::UNHANDLED, event_result);

  // DispatchKeyEventPostIME() should not have beeen called. In production
  // the following calls result:
  // InputMethodChromeOS -> RemoteTextInputClient -> TextInputClientImpl ->
  // InputMethodMus's delegate.
  EXPECT_FALSE(input_method_delegate.was_dispatch_key_event_post_ime_called());
}

}  // namespace aura
