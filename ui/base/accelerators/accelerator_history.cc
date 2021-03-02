// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator_history.h"

#include "ui/events/event.h"
#include "ui/events/event_target.h"

namespace ui {

namespace {

bool ShouldFilter(ui::KeyEvent* event) {
  const ui::EventType type = event->type();
  if (!event->target() ||
      (type != ui::ET_KEY_PRESSED && type != ui::ET_KEY_RELEASED) ||
      event->is_char() || !event->target() ||
      // Key events with key code of VKEY_PROCESSKEY, usually created by virtual
      // keyboard (like handwriting input), have no effect on accelerator and
      // they may disturb the accelerator history. So filter them out. (see
      // https://crbug.com/918317)
      event->key_code() == ui::VKEY_PROCESSKEY) {
    return true;
  }

  return false;
}

}  // namespace

AcceleratorHistory::AcceleratorHistory() = default;

AcceleratorHistory::~AcceleratorHistory() = default;

void AcceleratorHistory::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(event->target());
  if (!ShouldFilter(event))
    StoreCurrentAccelerator(ui::Accelerator(*event));
}

void AcceleratorHistory::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED ||
      event->type() == ui::ET_MOUSE_RELEASED) {
    InterruptCurrentAccelerator();
  }
}

void AcceleratorHistory::StoreCurrentAccelerator(
    const Accelerator& accelerator) {
  // Track the currently pressed keys so that we don't mistakenly store an
  // already pressed key as a new keypress after another key has been released.
  // As an example, when the user presses and holds Alt+Search, then releases
  // Alt but keeps holding the Search key down, at this point no new Search
  // presses should be stored in the history after the Alt release, since Search
  // was never released in the first place. crbug.com/704280.
  if (accelerator.key_state() == Accelerator::KeyState::PRESSED) {
    if (!currently_pressed_keys_.emplace(accelerator.key_code()).second)
      return;
  } else {
    currently_pressed_keys_.erase(accelerator.key_code());
  }

  if (accelerator != current_accelerator_) {
    previous_accelerator_ = current_accelerator_;
    current_accelerator_ = accelerator;
  }
}

void AcceleratorHistory::InterruptCurrentAccelerator() {
  if (current_accelerator_.key_state() == Accelerator::KeyState::PRESSED) {
    // Only interrupts pressed keys.
    current_accelerator_.set_interrupted_by_mouse_event(true);
  }
}

} // namespace ui
