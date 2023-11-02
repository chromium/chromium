// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYBOARD_EVENT_COUNTER_H_
#define UI_EVENTS_KEYBOARD_EVENT_COUNTER_H_

#include <stddef.h>

#include <atomic>
#include <set>

#include "ui/events/events_export.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"

namespace ui {

// This class tracks the total number of keypresses based on the OnKeyboardEvent
// calls it receives from the client.
// Multiple key down events for the same key are counted as one keypress until
// the same key is released.
class EVENTS_EXPORT KeyboardEventCounter {
 public:
  KeyboardEventCounter();

  KeyboardEventCounter(const KeyboardEventCounter&) = delete;
  KeyboardEventCounter& operator=(const KeyboardEventCounter&) = delete;

  ~KeyboardEventCounter();

  // Returns the total number of keypresses since its creation.
  // Can be called on any thread.
  uint32_t GetKeyPressCount() const;

  // The client should call this method on key down or key up events.
  // Must be called on a single thread.
  void OnKeyboardEvent(EventType event, KeyboardCode key_code);

 private:
  // The set of keys currently held down.
  std::set<KeyboardCode> pressed_keys_;

  std::atomic<uint32_t> total_key_presses_;
};

}  // namespace ui

#endif  // UI_EVENTS_KEYBOARD_EVENT_COUNTER_H_
