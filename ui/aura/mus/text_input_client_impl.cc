// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/text_input_client_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"
#include "ui/aura/mus/input_method_mus.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event_utils.h"

namespace aura {
namespace {

// Called when event processing processes an event. |callback| is the callback
// supplied to DispatchKeyEventPostIMECallback() and |handled| is true if the
// event was handled.
void OnKeyEventProcessed(
    ws::mojom::TextInputClient::DispatchKeyEventPostIMECallback callback,
    bool handled) {
  if (callback)
    std::move(callback).Run(handled);
}

}  // namespace

TextInputClientImpl::TextInputClientImpl(
    ui::TextInputClient* text_input_client,
    ui::internal::InputMethodDelegate* delegate)
    : text_input_client_(text_input_client),
      binding_(this),
      delegate_(delegate) {}

TextInputClientImpl::~TextInputClientImpl() = default;

ws::mojom::TextInputClientPtr TextInputClientImpl::CreateInterfacePtrAndBind() {
  ws::mojom::TextInputClientPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void TextInputClientImpl::SetCompositionText(
    const ui::CompositionText& composition) {
  text_input_client_->SetCompositionText(composition);
}

void TextInputClientImpl::ConfirmCompositionText() {
  text_input_client_->ConfirmCompositionText();
}

void TextInputClientImpl::ClearCompositionText() {
  text_input_client_->ClearCompositionText();
}

void TextInputClientImpl::InsertText(const base::string16& text) {
  text_input_client_->InsertText(text);
}

void TextInputClientImpl::InsertChar(std::unique_ptr<ui::Event> event) {
  DCHECK(event->IsKeyEvent());
  text_input_client_->InsertChar(*event->AsKeyEvent());
}

void TextInputClientImpl::DispatchKeyEventPostIME(
    std::unique_ptr<ui::Event> event,
    DispatchKeyEventPostIMECallback callback) {
  if (!delegate_) {
    if (callback)
      std::move(callback).Run(false);
    return;
  }
  ui::KeyEvent* key_event = event->AsKeyEvent();

  // Install a callback on the event. This way if async processing happens,
  // |callback| is notified appropriately.
  ui::KeyEvent::KeyDispatcherApi(key_event).set_async_callback(
      base::BindOnce(&OnKeyEventProcessed, std::move(callback)));
  // Use a null callback as the async_callback previously set takes care of
  // notifying |callback|.
  delegate_->DispatchKeyEventPostIME(event->AsKeyEvent(), base::NullCallback());

  if (!key_event->HasAsyncCallback())
    return;  // Event is being processed async.

  // The delegate finished processing the event. Run the ack now.
  const bool handled = key_event->handled();
  key_event->WillHandleAsync().Run(handled);
}

}  // namespace aura
