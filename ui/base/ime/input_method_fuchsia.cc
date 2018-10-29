// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_fuchsia.h"

#include <fuchsia/ui/input/cpp/fidl.h>
#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/fuchsia/component_context.h"
#include "ui/base/ime/fuchsia/input_method_keyboard_controller_fuchsia.h"
#include "ui/base/ime/text_input_client.h"

namespace ui {

InputMethodFuchsia::InputMethodFuchsia(internal::InputMethodDelegate* delegate)
    : InputMethodBase(delegate) {
  virtual_keyboard_controller_ =
      std::make_unique<InputMethodKeyboardControllerFuchsia>(this);
}

InputMethodFuchsia::~InputMethodFuchsia() {}

InputMethodKeyboardController*
InputMethodFuchsia::GetInputMethodKeyboardController() {
  return virtual_keyboard_controller_.get();
}

void InputMethodFuchsia::DispatchEvent(ui::Event* event) {
  DCHECK(event->IsKeyEvent());
  DispatchKeyEvent(event->AsKeyEvent());
}

ui::EventDispatchDetails InputMethodFuchsia::DispatchKeyEvent(
    ui::KeyEvent* event) {
  DCHECK(event->type() == ET_KEY_PRESSED || event->type() == ET_KEY_RELEASED);

  // If no text input client, do nothing.
  if (!GetTextInputClient())
    return DispatchKeyEventPostIME(event, base::NullCallback());

  // Insert the character.
  ui::EventDispatchDetails dispatch_details =
      DispatchKeyEventPostIME(event, base::NullCallback());
  if (!event->stopped_propagation() && !dispatch_details.dispatcher_destroyed &&
      event->type() == ET_KEY_PRESSED && GetTextInputClient()) {
    const uint16_t ch = event->GetCharacter();
    if (ch) {
      GetTextInputClient()->InsertChar(*event);
      event->StopPropagation();
    }
  }
  return dispatch_details;
}

void InputMethodFuchsia::OnCaretBoundsChanged(const TextInputClient* client) {}

void InputMethodFuchsia::CancelComposition(const TextInputClient* client) {}

bool InputMethodFuchsia::IsCandidatePopupOpen() const {
  return false;
}

}  // namespace ui
