// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fuchsia/input_method_fuchsia.h"

#include <fuchsia/ui/input/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <memory>
#include <utility>

#include "base/fuchsia/process_context.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

InputMethodFuchsia::InputMethodFuchsia(internal::InputMethodDelegate* delegate,
                                       fuchsia::ui::views::ViewRef view_ref)
    : InputMethodBase(delegate),
      virtual_keyboard_controller_(std::move(view_ref), this) {}

InputMethodFuchsia::~InputMethodFuchsia() {}

VirtualKeyboardController* InputMethodFuchsia::GetVirtualKeyboardController() {
  return &virtual_keyboard_controller_;
}

ui::EventDispatchDetails InputMethodFuchsia::DispatchKeyEvent(
    ui::KeyEvent* event) {
  DCHECK(event->type() == ET_KEY_PRESSED || event->type() == ET_KEY_RELEASED);

  // If no text input client, do nothing.
  if (!GetTextInputClient())
    return DispatchKeyEventPostIME(event);

  // Insert the character.
  ui::EventDispatchDetails dispatch_details = DispatchKeyEventPostIME(event);
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
