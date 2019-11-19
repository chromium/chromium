// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mock_input_method.h"

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "build/build_config.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/events/event.h"

namespace ui {

MockInputMethod::MockInputMethod(internal::InputMethodDelegate* delegate)
    : text_input_client_(NULL), delegate_(delegate) {
}

MockInputMethod::~MockInputMethod() {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnInputMethodDestroyed(this);
}

void MockInputMethod::SetDelegate(internal::InputMethodDelegate* delegate) {
  delegate_ = delegate;
}

void MockInputMethod::SetFocusedTextInputClient(TextInputClient* client) {
  if (text_input_client_ == client)
    return;
  text_input_client_ = client;
  if (client)
    OnTextInputTypeChanged(client);
}

void MockInputMethod::DetachTextInputClient(TextInputClient* client) {
  if (text_input_client_ == client) {
    text_input_client_ = NULL;
  }
}

TextInputClient* MockInputMethod::GetTextInputClient() const {
  return text_input_client_;
}

ui::EventDispatchDetails MockInputMethod::DispatchKeyEvent(
    ui::KeyEvent* event) {
  return delegate_->DispatchKeyEventPostIME(event);
}

void MockInputMethod::OnFocus() {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnFocus();
}

void MockInputMethod::OnBlur() {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnBlur();
}

#if defined(OS_WIN)
bool MockInputMethod::OnUntranslatedIMEMessage(const MSG event,
                                               NativeEventResult* result) {
  if (result)
    *result = NativeEventResult();
  return false;
}
#endif

void MockInputMethod::OnTextInputTypeChanged(const TextInputClient* client) {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnTextInputStateChanged(client);
}

void MockInputMethod::OnCaretBoundsChanged(const TextInputClient* client) {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnCaretBoundsChanged(client);
}

void MockInputMethod::CancelComposition(const TextInputClient* client) {
}

void MockInputMethod::OnInputLocaleChanged() {
}

bool MockInputMethod::IsInputLocaleCJK() const {
  return false;
}

TextInputType MockInputMethod::GetTextInputType() const {
  return TEXT_INPUT_TYPE_NONE;
}

TextInputMode MockInputMethod::GetTextInputMode() const {
  return TEXT_INPUT_MODE_DEFAULT;
}

int MockInputMethod::GetTextInputFlags() const {
  return 0;
}

bool MockInputMethod::CanComposeInline() const {
  return true;
}

bool MockInputMethod::IsCandidatePopupOpen() const {
  return false;
}

bool MockInputMethod::GetClientShouldDoLearning() {
  return false;
}

void MockInputMethod::ShowVirtualKeyboardIfEnabled() {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnShowVirtualKeyboardIfEnabled();
}

void MockInputMethod::AddObserver(InputMethodObserver* observer) {
  observer_list_.AddObserver(observer);
}

void MockInputMethod::RemoveObserver(InputMethodObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

InputMethodKeyboardController*
MockInputMethod::GetInputMethodKeyboardController() {
  return &keyboard_controller_;
}

const std::vector<std::unique_ptr<ui::KeyEvent>>&
MockInputMethod::GetKeyEventsForTesting() {
  return key_events_for_testing_;
}

}  // namespace ui
