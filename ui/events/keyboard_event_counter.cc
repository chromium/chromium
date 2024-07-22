// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keyboard_event_counter.h"

#include "base/check_op.h"

namespace ui {

KeyboardEventCounter::KeyboardEventCounter() : total_key_presses_(0) {}

KeyboardEventCounter::~KeyboardEventCounter() = default;

void KeyboardEventCounter::OnKeyboardEvent(EventType event,
                                           KeyboardCode key_code) {
  // Updates the pressed keys and the total count of key presses.
  if (event == EventType::kKeyPressed) {
    if (pressed_keys_.find(key_code) != pressed_keys_.end())
      return;
    pressed_keys_.insert(key_code);
    ++total_key_presses_;
  } else {
    DCHECK_EQ(EventType::kKeyReleased, event);
    pressed_keys_.erase(key_code);
  }
}

uint32_t KeyboardEventCounter::GetKeyPressCount() const {
  return total_key_presses_.load();
}

}  // namespace ui
