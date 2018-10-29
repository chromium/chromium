// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/input_method_mus.h"

#include <utility>

#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/mus/input_method_mus_delegate.h"
#include "ui/aura/mus/text_input_client_impl.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/platform_window/mojo/ime_type_converters.h"
#include "ui/platform_window/mojo/text_input_state.mojom.h"

using ws::mojom::EventResult;

namespace aura {
namespace {

void CallEventResultCallback(InputMethodMus::EventResultCallback ack_callback,
                             bool handled) {
  // |ack_callback| can be null if the standard form of DispatchKeyEvent() is
  // called instead of the version which provides a callback. In mus+ash we
  // use the version with callback, but some unittests use the standard form.
  if (!ack_callback)
    return;

  std::move(ack_callback)
      .Run(handled ? EventResult::HANDLED : EventResult::UNHANDLED);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// InputMethodMus, public:

InputMethodMus::InputMethodMus(
    ui::internal::InputMethodDelegate* delegate,
    InputMethodMusDelegate* input_method_mus_delegate)
    : input_method_mus_delegate_(input_method_mus_delegate) {
  SetDelegate(delegate);
}

InputMethodMus::~InputMethodMus() {
  // Mus won't dispatch the next key event until the existing one is acked. We
  // may have KeyEvents sent to IME and awaiting the result, we need to ack
  // them otherwise mus won't process the next event until it times out.
  AckPendingCallbacks();
}

void InputMethodMus::Init(service_manager::Connector* connector) {
  if (connector)
    connector->BindInterface(ws::mojom::kServiceName, &ime_driver_);
}

ui::EventDispatchDetails InputMethodMus::DispatchKeyEvent(
    ui::KeyEvent* event,
    EventResultCallback ack_callback) {
  DCHECK(event->type() == ui::ET_KEY_PRESSED ||
         event->type() == ui::ET_KEY_RELEASED);

  // If no text input client, do nothing.
  if (!GetTextInputClient()) {
    return DispatchKeyEventPostIME(
        event,
        base::BindOnce(&CallEventResultCallback, std::move(ack_callback)));
  }

  return SendKeyEventToInputMethod(*event, std::move(ack_callback));
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodMus, ui::InputMethod implementation:

void InputMethodMus::OnFocus() {
  InputMethodBase::OnFocus();
  UpdateTextInputType();
}

void InputMethodMus::OnBlur() {
  InputMethodBase::OnBlur();
  UpdateTextInputType();
}

ui::EventDispatchDetails InputMethodMus::DispatchKeyEvent(ui::KeyEvent* event) {
  ui::EventDispatchDetails dispatch_details =
      DispatchKeyEvent(event, EventResultCallback());
  // Mark the event as handled so that EventGenerator doesn't attempt to
  // deliver event as well.
  event->SetHandled();
  return dispatch_details;
}

void InputMethodMus::OnTextInputTypeChanged(const ui::TextInputClient* client) {
  InputMethodBase::OnTextInputTypeChanged(client);
  if (!IsTextInputClientFocused(client))
    return;

  UpdateTextInputType();

  if (input_method_)
    input_method_->OnTextInputTypeChanged(client->GetTextInputType());
}

void InputMethodMus::OnCaretBoundsChanged(const ui::TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;

  if (input_method_)
    input_method_->OnCaretBoundsChanged(client->GetCaretBounds());

  NotifyTextInputCaretBoundsChanged(client);
}

void InputMethodMus::CancelComposition(const ui::TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;

  if (input_method_)
    input_method_->CancelComposition();
}

void InputMethodMus::OnInputLocaleChanged() {
  // TODO(moshayedi): crbug.com/637418. Not supported in ChromeOS. Investigate
  // whether we want to support this or not.
  NOTIMPLEMENTED_LOG_ONCE();
}

bool InputMethodMus::IsCandidatePopupOpen() const {
  // TODO(moshayedi): crbug.com/637416. Implement this properly when we have a
  // mean for displaying candidate list popup.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void InputMethodMus::ShowVirtualKeyboardIfEnabled() {
  InputMethodBase::ShowVirtualKeyboardIfEnabled();
  if (input_method_)
    input_method_->ShowVirtualKeyboardIfEnabled();
}

ui::EventDispatchDetails InputMethodMus::SendKeyEventToInputMethod(
    const ui::KeyEvent& event,
    EventResultCallback ack_callback) {
  if (!input_method_) {
    // This code path is hit in tests that don't connect to the server.
    DCHECK(!ack_callback);
    std::unique_ptr<ui::Event> event_clone = ui::Event::Clone(event);
    return DispatchKeyEventPostIME(event_clone->AsKeyEvent(),
                                   base::NullCallback());
  }

  // IME driver will notify us whether it handled the event or not by calling
  // ProcessKeyEventCallback(), in which we will run the |ack_callback| to tell
  // the window server if client handled the event or not.
  pending_callbacks_.push_back(std::move(ack_callback));
  input_method_->ProcessKeyEvent(
      ui::Event::Clone(event),
      base::BindOnce(&InputMethodMus::ProcessKeyEventCallback,
                     base::Unretained(this), event));

  return ui::EventDispatchDetails();
}

void InputMethodMus::OnDidChangeFocusedClient(
    ui::TextInputClient* focused_before,
    ui::TextInputClient* focused) {
  InputMethodBase::OnDidChangeFocusedClient(focused_before, focused);
  UpdateTextInputType();

  // We are about to close the pipe with pending callbacks. Closing the pipe
  // results in none of the callbacks being run. We have to run the callbacks
  // else mus won't process the next event immediately.
  AckPendingCallbacks();

  if (!focused) {
    input_method_ = nullptr;
    input_method_ptr_.reset();
    text_input_client_.reset();
    return;
  }

  text_input_client_ =
      std::make_unique<TextInputClientImpl>(focused, delegate());

  if (ime_driver_) {
    ws::mojom::StartSessionDetailsPtr details =
        ws::mojom::StartSessionDetails::New();
    details->client =
        text_input_client_->CreateInterfacePtrAndBind().PassInterface();
    details->input_method_request = MakeRequest(&input_method_ptr_);
    input_method_ = input_method_ptr_.get();
    details->text_input_type = focused->GetTextInputType();
    details->text_input_mode = focused->GetTextInputMode();
    details->text_direction = focused->GetTextDirection();
    details->text_input_flags = focused->GetTextInputFlags();
    details->caret_bounds = focused->GetCaretBounds();
    ime_driver_->StartSession(std::move(details));
  }
}

void InputMethodMus::UpdateTextInputType() {
  ui::TextInputType type = GetTextInputType();
  ui::mojom::TextInputStatePtr state = ui::mojom::TextInputState::New();
  state->type = mojo::ConvertTo<ui::mojom::TextInputType>(type);
  if (input_method_mus_delegate_) {
    if (type != ui::TEXT_INPUT_TYPE_NONE)
      input_method_mus_delegate_->SetImeVisibility(true, std::move(state));
    else
      input_method_mus_delegate_->SetTextInputState(std::move(state));
  }
}

void InputMethodMus::AckPendingCallbacks() {
  for (auto& callback : pending_callbacks_) {
    if (callback)
      std::move(callback).Run(EventResult::HANDLED);
  }
  pending_callbacks_.clear();
}

void InputMethodMus::ProcessKeyEventCallback(
    const ui::KeyEvent& event,
    bool handled) {
  // Remove the callback as DispatchKeyEventPostIME() may lead to calling
  // AckPendingCallbacksUnhandled(), which mutates |pending_callbacks_|.
  DCHECK(!pending_callbacks_.empty());
  EventResultCallback ack_callback = std::move(pending_callbacks_.front());
  pending_callbacks_.pop_front();
  CallEventResultCallback(std::move(ack_callback), handled);
}

}  // namespace aura
