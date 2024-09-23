// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/caps_lock_event_rewriter.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "ui/base/accelerators/ash/right_alt_event_property.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/event_property.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {

CapsLockEventRewriter::CapsLockEventRewriter(
    KeyboardLayoutEngine* keyboard_layout_engine,
    KeyboardCapability* keyboard_capability,
    ash::input_method::ImeKeyboard* ime_keyboard)
    : keyboard_layout_engine_(keyboard_layout_engine),
      keyboard_capability_(keyboard_capability),
      ime_keyboard_(ime_keyboard) {}
CapsLockEventRewriter::~CapsLockEventRewriter() = default;

EventDispatchDetails CapsLockEventRewriter::RewriteEvent(
    const Event& event,
    const Continuation continuation) {
  std::unique_ptr<Event> rewritten_event;
  switch (event.type()) {
    case EventType::kKeyPressed: {
      rewritten_event = RewritePressKeyEvent(*event.AsKeyEvent());
      break;
    }
    case EventType::kKeyReleased: {
      rewritten_event = RewriteReleaseKeyEvent(*event.AsKeyEvent());
      break;
    }
    default: {
      // Update flags by reconstructing them from the modifier key status.
      const int flags = event.flags();
      const int rewritten_flags = RewriteModifierFlags(event.flags());
      if (flags != rewritten_flags) {
        rewritten_event = event.Clone();

        // SetNativeEvent must be called explicitly as native events are not
        // copied on ChromeOS by default. This is because `PlatformEvent` is a
        // pointer by default, so its lifetime can not be guaranteed in general.
        // In this case, the lifetime of  `rewritten_event` is guaranteed to be
        // less than the original `event`.
        SetNativeEvent(*rewritten_event, event.native_event());

        // Note: this updates DomKey to reflect the new flags.
        rewritten_event->SetFlags(rewritten_flags);
      }
      break;
    }
  }

  return SendEvent(continuation,
                   rewritten_event ? rewritten_event.get() : &event);
}

std::unique_ptr<KeyEvent> CapsLockEventRewriter::RewritePressKeyEvent(
    const KeyEvent& key_event) {
  internal::PhysicalKey physical_key{key_event.code(),
                                     GetKeyboardDeviceIdProperty(key_event)};

  // If this is a repeat event, remap it the same as the original press.
  if (auto it = remapped_keys_.find(physical_key); it != remapped_keys_.end()) {
    return BuildRewrittenEvent(key_event, it->second);
  }

  RemappedKey remapped_key = {key_event.code(), key_event.GetDomKey(),
                              key_event.key_code()};

  const bool is_right_alt_key = key_event.code() == DomCode::LAUNCH_ASSISTANT &&
                                HasRightAltProperty(key_event);
  const bool is_function_down = (key_event.flags() & EF_FUNCTION_DOWN) != 0;
  if (is_right_alt_key && is_function_down) {
    // Update DomKey and KeyboardCode respecting the current keyboard layout.
    const DomCode remapped_dom_code = DomCode::CAPS_LOCK;
    DomKey dom_key;
    KeyboardCode key_code;
    if (!keyboard_layout_engine_->Lookup(remapped_dom_code, key_event.flags(),
                                         &dom_key, &key_code)) {
      LOG(ERROR) << "Failed to look up keyboard layout";
      return nullptr;
    }
    remapped_key = {remapped_dom_code, dom_key, key_code,
                    /*should_remove_function_modifier=*/true};
    remapped_keys_.insert_or_assign(physical_key, remapped_key);
  }

  // When the keyboard rewriter fix is enabled, the `CapsLockEventRewriter`
  // is responsible for toggling CapsLock when CapsLock DomKeys are
  // encountered in the input stream. These can originate from other event
  // rewriters as well as physical CapsLock keys.
  if (remapped_key.key == DomKey::CAPS_LOCK) {
    if (pressed_modifier_keys_.insert_or_assign(physical_key, EF_MOD3_DOWN)
            .second) {
      ime_keyboard_->SetCapsLockEnabled(!ime_keyboard_->IsCapsLockEnabled());
    }
  }

  return BuildRewrittenEvent(key_event, remapped_key);
}

std::unique_ptr<KeyEvent> CapsLockEventRewriter::RewriteReleaseKeyEvent(
    const KeyEvent& event) {
  int device_id = GetKeyboardDeviceIdProperty(event);
  internal::PhysicalKey physical_key{event.code(), device_id};
  pressed_modifier_keys_.erase(physical_key);

  // Instead of looking up the remap rule again here, we'll just reuse the remap
  // data on the pressed event, so that this release event is remapped in
  // the same way with the pressed event.
  std::optional<RemappedKey> remapped;
  if (auto it = remapped_keys_.find(physical_key); it != remapped_keys_.end()) {
    remapped = it->second;
    remapped_keys_.erase(it);
  }

  return BuildRewrittenEvent(
      event, remapped.value_or(RemappedKey{event.code(), event.GetDomKey(),
                                           event.key_code()}));
}

std::unique_ptr<KeyEvent> CapsLockEventRewriter::BuildRewrittenEvent(
    const KeyEvent& event,
    const RemappedKey& remapped) {
  // Get rewritten flags removing the function modifier if it should be removed.
  const EventFlags flags = RewriteModifierFlags(
      event.flags() &
      ~(remapped.should_remove_function_modifier ? EF_FUNCTION_DOWN : EF_NONE));
  if (remapped.code == event.code() && remapped.key == event.GetDomKey() &&
      remapped.key_code == event.key_code() && flags == event.flags()) {
    // Nothing is rewritten.
    return nullptr;
  }

  auto rewritten_event =
      std::make_unique<KeyEvent>(event.type(), remapped.key_code, remapped.code,
                                 flags, remapped.key, event.time_stamp());
  rewritten_event->set_scan_code(event.scan_code());
  rewritten_event->set_source_device_id(event.source_device_id());
  return rewritten_event;
}

EventFlags CapsLockEventRewriter::RewriteModifierFlags(EventFlags flags) const {
  // Only the capslock modifier flag can be removed based on the set of
  // currently pressed keys. Will be readded based on the current capslock
  // state.
  constexpr EventFlags kTargetModifierFlags = EF_MOD3_DOWN | EF_CAPS_LOCK_ON;
  flags &= ~kTargetModifierFlags;

  // Recalculate modifier flags from the currently pressed keys.
  for (const auto& [_, modifier] : pressed_modifier_keys_) {
    flags |= modifier;
  }

  // Update CapsLock.
  if (ime_keyboard_->IsCapsLockEnabled()) {
    flags |= EF_CAPS_LOCK_ON;
  }

  return flags;
}

}  // namespace ui
