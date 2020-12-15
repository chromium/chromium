// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_base.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/virtual_keyboard_controller_stub.h"
#include "ui/events/event.h"

namespace ui {

InputMethodBase::InputMethodBase(internal::InputMethodDelegate* delegate)
    : InputMethodBase(delegate, nullptr) {}

InputMethodBase::InputMethodBase(
    internal::InputMethodDelegate* delegate,
    std::unique_ptr<VirtualKeyboardController> keyboard_controller)
    : delegate_(delegate),
      keyboard_controller_(std::move(keyboard_controller)) {}

InputMethodBase::~InputMethodBase() {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnInputMethodDestroyed(this);
}

void InputMethodBase::SetDelegate(internal::InputMethodDelegate* delegate) {
  delegate_ = delegate;
}

void InputMethodBase::OnFocus() {
}

void InputMethodBase::OnBlur() {
}

#if defined(OS_WIN)
bool InputMethodBase::OnUntranslatedIMEMessage(
    const MSG event,
    InputMethod::NativeEventResult* result) {
  return false;
}
#endif

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

void InputMethodBase::SetOnScreenKeyboardBounds(const gfx::Rect& new_bounds) {
  keyboard_bounds_ = new_bounds;
  if (text_input_client_)
    text_input_client_->EnsureCaretNotInRect(keyboard_bounds_);
}

void InputMethodBase::OnTextInputTypeChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  NotifyTextInputStateChanged(client);
}

void InputMethodBase::OnInputLocaleChanged() {
}

bool InputMethodBase::IsInputLocaleCJK() const {
  return false;
}

TextInputType InputMethodBase::GetTextInputType() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputType() : TEXT_INPUT_TYPE_NONE;
}

TextInputMode InputMethodBase::GetTextInputMode() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputMode() : TEXT_INPUT_MODE_DEFAULT;
}

int InputMethodBase::GetTextInputFlags() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputFlags() : 0;
}

bool InputMethodBase::CanComposeInline() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->CanComposeInline() : true;
}

bool InputMethodBase::GetClientShouldDoLearning() {
  TextInputClient* client = GetTextInputClient();
  return client && client->ShouldDoLearning();
}

void InputMethodBase::ShowVirtualKeyboardIfEnabled() {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnShowVirtualKeyboardIfEnabled();
  if (auto* keyboard = GetVirtualKeyboardController())
    keyboard->DisplayVirtualKeyboard();
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
  return delegate_ ? delegate_->DispatchKeyEventPostIME(event)
                   : ui::EventDispatchDetails();
}

void InputMethodBase::NotifyTextInputStateChanged(
    const TextInputClient* client) {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnTextInputStateChanged(client);
}

void InputMethodBase::NotifyTextInputCaretBoundsChanged(
    const TextInputClient* client) {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnCaretBoundsChanged(client);
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
  if (text_input_client_)
    text_input_client_->EnsureCaretNotInRect(keyboard_bounds_);
}

std::vector<gfx::Rect> InputMethodBase::GetCompositionBounds(
    const TextInputClient* client) {
  std::vector<gfx::Rect> bounds;
  if (client->HasCompositionText()) {
    uint32_t i = 0;
    gfx::Rect rect;
    while (client->GetCompositionCharacterBounds(i++, &rect))
      bounds.push_back(rect);
  } else {
    // For case of no composition at present, use caret bounds which is required
    // by the IME extension for certain features (e.g. physical keyboard
    // auto-correct).
    bounds.push_back(client->GetCaretBounds());
  }
  return bounds;
}

bool InputMethodBase::SendFakeProcessKeyEvent(bool pressed) const {
  KeyEvent evt(pressed ? ET_KEY_PRESSED : ET_KEY_RELEASED,
               pressed ? VKEY_PROCESSKEY : VKEY_UNKNOWN, EF_IME_FABRICATED_KEY);
  ignore_result(DispatchKeyEventPostIME(&evt));
  return evt.stopped_propagation();
}

}  // namespace ui
