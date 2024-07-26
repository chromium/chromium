// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/event_modifiers.h"

#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ui {

namespace {

static const int kEventFlagFromModifiers[] = {
    EF_NONE,                  // MODIFIER_NONE,
    EF_SHIFT_DOWN,            // MODIFIER_SHIFT
    EF_CONTROL_DOWN,          // MODIFIER_CONTROL
    EF_ALT_DOWN,              // MODIFIER_ALT
    EF_COMMAND_DOWN,          // MODIFIER_COMMAND
    EF_ALTGR_DOWN,            // MODIFIER_ALTGR
    EF_MOD3_DOWN,             // MODIFIER_MOD3
    EF_CAPS_LOCK_ON,          // MODIFIER_CAPS_LOCK
    EF_LEFT_MOUSE_BUTTON,     // MODIFIER_LEFT_MOUSE_BUTTON
    EF_MIDDLE_MOUSE_BUTTON,   // MODIFIER_MIDDLE_MOUSE_BUTTON
    EF_RIGHT_MOUSE_BUTTON,    // MODIFIER_RIGHT_MOUSE_BUTTON
    EF_BACK_MOUSE_BUTTON,     // MODIFIER_BACK_MOUSE_BUTTON
    EF_FORWARD_MOUSE_BUTTON,  // MODIFIER_FORWARD_MOUSE_BUTTON
    EF_FUNCTION_DOWN,         // MODIFIER_FUNCTION
};

}  // namespace

EventModifiers::EventModifiers() {
  memset(modifiers_down_, 0, sizeof(modifiers_down_));
}
EventModifiers::~EventModifiers() {}

void EventModifiers::UpdateModifier(unsigned int modifier, bool down) {
  DCHECK_LT(modifier, static_cast<unsigned int>(MODIFIER_NUM_MODIFIERS));

  if (down) {
    modifiers_down_[modifier]++;
  } else {
    // Ignore spurious modifier "up" events. This might happen if the
    // button is down during startup.
    if (modifiers_down_[modifier])
      modifiers_down_[modifier]--;
  }

  UpdateFlags(modifier);
}

void EventModifiers::UpdateModifierLock(unsigned int modifier, bool down) {
  DCHECK_LT(modifier, static_cast<unsigned int>(MODIFIER_NUM_MODIFIERS));

  if (down)
    modifier_flags_locked_ ^= kEventFlagFromModifiers[modifier];

  UpdateFlags(modifier);
}

void EventModifiers::SetModifierLock(unsigned int modifier, bool locked) {
  DCHECK_LT(modifier, static_cast<unsigned int>(MODIFIER_NUM_MODIFIERS));

  if (locked)
    modifier_flags_locked_ |= kEventFlagFromModifiers[modifier];
  else
    modifier_flags_locked_ &= ~kEventFlagFromModifiers[modifier];

  UpdateFlags(modifier);
}

void EventModifiers::UpdateFlags(unsigned int modifier) {
  int mask = kEventFlagFromModifiers[modifier];
  bool down = modifiers_down_[modifier] != 0;
  bool locked = (modifier_flags_locked_ & mask) != 0;
  if (down != locked)
    modifier_flags_ |= mask;
  else
    modifier_flags_ &= ~mask;
}

int EventModifiers::GetModifierFlags() {
  return modifier_flags_;
}

void EventModifiers::ResetKeyboardModifiers() {
  static const int kKeyboardModifiers[] = {MODIFIER_SHIFT, MODIFIER_CONTROL,
                                           MODIFIER_ALT,   MODIFIER_COMMAND,
                                           MODIFIER_ALTGR, MODIFIER_MOD3};
  for (const int modifier : kKeyboardModifiers) {
    modifiers_down_[modifier] = 0;
    UpdateFlags(modifier);
  }
}

// static
int EventModifiers::GetEventFlagFromModifier(unsigned int modifier) {
  return kEventFlagFromModifiers[modifier];
}

// static
int EventModifiers::GetModifierFromEventFlag(int flag) {
  switch (flag) {
    case EF_SHIFT_DOWN:
      return MODIFIER_SHIFT;
    case EF_CONTROL_DOWN:
      return MODIFIER_CONTROL;
    case EF_ALT_DOWN:
      return MODIFIER_ALT;
    case EF_COMMAND_DOWN:
      return MODIFIER_COMMAND;
    case EF_ALTGR_DOWN:
      return MODIFIER_ALTGR;
    case EF_MOD3_DOWN:
      return MODIFIER_MOD3;
    case EF_FUNCTION_DOWN:
      return MODIFIER_FUNCTION;
    case EF_CAPS_LOCK_ON:
      return MODIFIER_CAPS_LOCK;
    case EF_LEFT_MOUSE_BUTTON:
      return MODIFIER_LEFT_MOUSE_BUTTON;
    case EF_MIDDLE_MOUSE_BUTTON:
      return MODIFIER_MIDDLE_MOUSE_BUTTON;
    case EF_RIGHT_MOUSE_BUTTON:
      return MODIFIER_RIGHT_MOUSE_BUTTON;
    case EF_BACK_MOUSE_BUTTON:
      return MODIFIER_BACK_MOUSE_BUTTON;
    case EF_FORWARD_MOUSE_BUTTON:
      return MODIFIER_FORWARD_MOUSE_BUTTON;
    default:
      return MODIFIER_NONE;
  }
}

}  // namespace ui
