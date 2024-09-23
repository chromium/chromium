// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_minimal.h"

#include <stdint.h>

#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"

namespace ui {

InputMethodMinimal::InputMethodMinimal(
    ImeKeyEventDispatcher* ime_key_event_dispatcher)
    : InputMethodBase(ime_key_event_dispatcher) {}

InputMethodMinimal::~InputMethodMinimal() {}

ui::EventDispatchDetails InputMethodMinimal::DispatchKeyEvent(
    ui::KeyEvent* event) {
  DCHECK(event->type() == EventType::kKeyPressed ||
         event->type() == EventType::kKeyReleased);

  // If no text input client, do nothing.
  if (!GetTextInputClient())
    return DispatchKeyEventPostIME(event);

  // Insert the character.
  ui::EventDispatchDetails dispatch_details = DispatchKeyEventPostIME(event);
  if (!event->stopped_propagation() && !dispatch_details.dispatcher_destroyed &&
      event->type() == EventType::kKeyPressed && GetTextInputClient()) {
    const uint16_t ch = event->GetCharacter();
    if (ch) {
      GetTextInputClient()->InsertChar(*event);
      event->StopPropagation();
    }
  }
  return dispatch_details;
}

void InputMethodMinimal::OnCaretBoundsChanged(const TextInputClient* client) {}

void InputMethodMinimal::CancelComposition(const TextInputClient* client) {}

bool InputMethodMinimal::IsCandidatePopupOpen() const {
  return false;
}

}  // namespace ui
