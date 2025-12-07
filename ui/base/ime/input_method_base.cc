// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_base.h"

#include <tuple>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/ime/virtual_keyboard_controller_stub.h"
#include "ui/events/event.h"

namespace ui {

InputMethodBase::InputMethodBase(
    ImeKeyEventDispatcher* ime_key_event_dispatcher)
    : InputMethodBase(ime_key_event_dispatcher, nullptr) {}

InputMethodBase::InputMethodBase(
    ImeKeyEventDispatcher* ime_key_event_dispatcher,
    std::unique_ptr<VirtualKeyboardController> keyboard_controller)
    : ime_key_event_dispatcher_(ime_key_event_dispatcher),
      keyboard_controller_(std::move(keyboard_controller)) {}

InputMethodBase::~InputMethodBase() {
  observer_list_.Notify(&InputMethodObserver::OnInputMethodDestroyed, this);
}

void InputMethodBase::SetImeKeyEventDispatcher(
    ImeKeyEventDispatcher* ime_key_event_dispatcher) {
  ime_key_event_dispatcher_ = ime_key_event_dispatcher;
}

void InputMethodBase::OnFocus() {
}

void InputMethodBase::OnBlur() {
}

void InputMethodBase::SetFocusedTextInputClient(TextInputClient* client) {
  SetFocusedTextInputClientInternal(client);
}

void InputMethodBase::DetachTextInputClient(TextInputClient* client) {
  if (text_input_client_ != client)
    return;
  SetFocusedTextInputClientInternal(nullptr);
}

TextInputClient* InputMethodBase::GetTextInputClient() const {
  return text_input_client_;
}

void InputMethodBase::SetVirtualKeyboardBounds(const gfx::Rect& new_bounds) {
  keyboard_bounds_ = new_bounds;
  if (text_input_client_)
    text_input_client_->EnsureCaretNotInRect(keyboard_bounds_);
}

void InputMethodBase::OnTextInputTypeChanged(TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  NotifyTextInputStateChanged(client);
}

TextInputType InputMethodBase::GetTextInputType() const {
  TextInputClient* client = GetTextInputClient();
  return client
             ? (client->GetTextInputFlags() & TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD
                    ? TEXT_INPUT_TYPE_PASSWORD
                    : client->GetTextInputType())
             : TEXT_INPUT_TYPE_NONE;
}

void InputMethodBase::SetVirtualKeyboardVisibilityIfEnabled(bool should_show) {
  observer_list_.Notify(
      &InputMethodObserver::OnVirtualKeyboardVisibilityChangedIfEnabled,
      should_show);
  auto* keyboard = GetVirtualKeyboardController();
  if (keyboard) {
    if (should_show) {
      keyboard->DisplayVirtualKeyboard();
    } else {
      keyboard->DismissVirtualKeyboard();
    }
  }
}

void InputMethodBase::AddObserver(InputMethodObserver* observer) {
  observer_list_.AddObserver(observer);
}

void InputMethodBase::RemoveObserver(InputMethodObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

VirtualKeyboardController* InputMethodBase::GetVirtualKeyboardController() {
  return keyboard_controller_.get();
}

void InputMethodBase::SetVirtualKeyboardControllerForTesting(
    std::unique_ptr<VirtualKeyboardController> controller) {
  keyboard_controller_ = std::move(controller);
}

bool InputMethodBase::IsTextInputClientFocused(const TextInputClient* client) {
  return client && (client == GetTextInputClient());
}

bool InputMethodBase::IsTextInputTypeNone() const {
  return GetTextInputType() == TEXT_INPUT_TYPE_NONE;
}

void InputMethodBase::OnInputMethodChanged() const {
  TextInputClient* client = GetTextInputClient();
  if (!IsTextInputTypeNone())
    client->OnInputMethodChanged();
}

ui::EventDispatchDetails InputMethodBase::DispatchKeyEventPostIME(
    ui::KeyEvent* event) const {
  if (text_input_client_) {
    text_input_client_->OnDispatchingKeyEventPostIME(event);
    if (event->handled())
      return EventDispatchDetails();
  }
  return ime_key_event_dispatcher_
             ? ime_key_event_dispatcher_->DispatchKeyEventPostIME(event)
             : ui::EventDispatchDetails();
}

void InputMethodBase::NotifyTextInputStateChanged(
    const TextInputClient* client) {
  observer_list_.Notify(&InputMethodObserver::OnTextInputStateChanged, client);
}

void InputMethodBase::NotifyTextInputCaretBoundsChanged(
    const TextInputClient* client) {
  observer_list_.Notify(&InputMethodObserver::OnCaretBoundsChanged, client);
}

void InputMethodBase::SetFocusedTextInputClientInternal(
    TextInputClient* client) {
  TextInputClient* old = text_input_client_;
  if (old == client)
    return;
  OnWillChangeFocusedClient(old, client);
  text_input_client_ = client;  // nullptr allowed.
  OnDidChangeFocusedClient(old, client);
  NotifyTextInputStateChanged(text_input_client_);

  // Move new focused window if necessary.
  if (text_input_client_ && !keyboard_bounds_.IsEmpty())
    text_input_client_->EnsureCaretNotInRect(keyboard_bounds_);
}

}  // namespace ui
