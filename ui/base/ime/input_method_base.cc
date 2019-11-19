// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_base.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_keyboard_controller_stub.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

namespace ui {

ui::IMEEngineHandlerInterface* InputMethodBase::GetEngine() {
  auto* bridge = ui::IMEBridge::Get();
  return bridge ? bridge->GetCurrentEngineHandler() : nullptr;
}

InputMethodBase::InputMethodBase(internal::InputMethodDelegate* delegate)
    : InputMethodBase(delegate, nullptr) {}

InputMethodBase::InputMethodBase(
    internal::InputMethodDelegate* delegate,
    std::unique_ptr<InputMethodKeyboardController> keyboard_controller)
    : delegate_(delegate),
      keyboard_controller_(std::move(keyboard_controller)) {}

InputMethodBase::~InputMethodBase() {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnInputMethodDestroyed(this);
  if (ui::IMEBridge::Get() &&
      ui::IMEBridge::Get()->GetInputContextHandler() == this)
    ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
}

void InputMethodBase::SetDelegate(internal::InputMethodDelegate* delegate) {
  delegate_ = delegate;
}

void InputMethodBase::OnFocus() {
  ui::IMEBridge* bridge = ui::IMEBridge::Get();
  if (bridge) {
    bridge->SetInputContextHandler(this);
    bridge->MaybeSwitchEngine();
  }
}

void InputMethodBase::OnBlur() {
  if (ui::IMEBridge::Get() &&
      ui::IMEBridge::Get()->GetInputContextHandler() == this)
    ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
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
  if (auto* keyboard = GetInputMethodKeyboardController())
    keyboard->DisplayVirtualKeyboard();
}

void InputMethodBase::AddObserver(InputMethodObserver* observer) {
  observer_list_.AddObserver(observer);
}

void InputMethodBase::RemoveObserver(InputMethodObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

InputMethodKeyboardController*
InputMethodBase::GetInputMethodKeyboardController() {
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

void InputMethodBase::CommitText(const std::string& text) {
  if (text.empty() || !GetTextInputClient() || IsTextInputTypeNone())
    return;

  const base::string16 utf16_text = base::UTF8ToUTF16(text);
  if (utf16_text.empty())
    return;

  if (!SendFakeProcessKeyEvent(true))
    GetTextInputClient()->InsertText(utf16_text);
  SendFakeProcessKeyEvent(false);
}

void InputMethodBase::UpdateCompositionText(const CompositionText& composition_,
                                            uint32_t cursor_pos,
                                            bool visible) {
  if (IsTextInputTypeNone())
    return;

  if (!SendFakeProcessKeyEvent(true)) {
    if (visible && !composition_.text.empty())
      GetTextInputClient()->SetCompositionText(composition_);
    else
      GetTextInputClient()->ClearCompositionText();
  }
  SendFakeProcessKeyEvent(false);
}

#if defined(OS_CHROMEOS)
bool InputMethodBase::SetCompositionRange(
    uint32_t before,
    uint32_t after,
    const std::vector<ui::ImeTextSpan>& text_spans) {
  return false;
}

bool InputMethodBase::SetSelectionRange(uint32_t start, uint32_t end) {
  return false;
}
#endif

void InputMethodBase::DeleteSurroundingText(int32_t offset, uint32_t length) {}

SurroundingTextInfo InputMethodBase::GetSurroundingTextInfo() {
  gfx::Range text_range;
  SurroundingTextInfo info;
  TextInputClient* client = GetTextInputClient();
  if (!client->GetTextRange(&text_range) ||
      !client->GetTextFromRange(text_range, &info.surrounding_text) ||
      !client->GetEditableSelectionRange(&info.selection_range)) {
    return SurroundingTextInfo();
  }
  // Makes the |selection_range| be relative to the |surrounding_text|.
  info.selection_range.set_start(info.selection_range.start() -
                                 text_range.start());
  info.selection_range.set_end(info.selection_range.end() - text_range.start());
  return info;
}

void InputMethodBase::SendKeyEvent(KeyEvent* event) {
  if (track_key_events_for_testing_) {
    key_events_for_testing_.push_back(std::make_unique<KeyEvent>(*event));
  }
  ui::EventDispatchDetails details = DispatchKeyEvent(event);
  DCHECK(!details.dispatcher_destroyed);
}

InputMethod* InputMethodBase::GetInputMethod() {
  return this;
}

void InputMethodBase::ConfirmCompositionText(bool reset_engine,
                                             bool keep_selection) {
  TextInputClient* client = GetTextInputClient();
  if (client && client->HasCompositionText())
    client->ConfirmCompositionText(keep_selection);
}

bool InputMethodBase::HasCompositionText() {
  TextInputClient* client = GetTextInputClient();
  return client && client->HasCompositionText();
}

const std::vector<std::unique_ptr<ui::KeyEvent>>&
InputMethodBase::GetKeyEventsForTesting() {
  return key_events_for_testing_;
}

}  // namespace ui
