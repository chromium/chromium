// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mac/input_method_mac.h"

#import <Cocoa/Cocoa.h>

namespace ui {

InputMethodMac::InputMethodMac(ImeKeyEventDispatcher* ime_key_event_dispatcher)
    : InputMethodBase(ime_key_event_dispatcher) {}

InputMethodMac::~InputMethodMac() = default;

ui::EventDispatchDetails InputMethodMac::DispatchKeyEvent(ui::KeyEvent* event) {
  // This is used on Mac only to dispatch events post-IME.
  return DispatchKeyEventPostIME(event);
}

void InputMethodMac::OnCaretBoundsChanged(const TextInputClient* client) {
}

void InputMethodMac::CancelComposition(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;

  [NSTextInputContext.currentInputContext discardMarkedText];
}

bool InputMethodMac::IsCandidatePopupOpen() const {
  // There seems to be no way to tell if a candidate window is open.
  return false;
}

}  // namespace ui
