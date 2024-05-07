// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_CAPS_LOCK_EVENT_REWRITER_H_
#define UI_EVENTS_ASH_CAPS_LOCK_EVENT_REWRITER_H_

#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/events/ash/event_rewriter_utils.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"

namespace ui {

// CapsLockEventRewriter rewrites Fn + RightAlt events into CapsLock.
class CapsLockEventRewriter : public EventRewriter {
 public:
  struct RemappedKey {
    DomCode code;
    DomKey key;
    KeyboardCode key_code;
    bool should_remove_function_modifier = false;
  };

  CapsLockEventRewriter(KeyboardLayoutEngine* keyboard_layout_engine,
                        KeyboardCapability* keyboard_capability,
                        ash::input_method::ImeKeyboard* ime_keyboard);
  CapsLockEventRewriter(const CapsLockEventRewriter&) = delete;
  CapsLockEventRewriter& operator=(const CapsLockEventRewriter&) = delete;
  ~CapsLockEventRewriter() override;

  // EventRewriter:
  EventDispatchDetails RewriteEvent(const Event& event,
                                    const Continuation continuation) override;

  std::unique_ptr<KeyEvent> RewritePressKeyEvent(const KeyEvent& key_event);
  std::unique_ptr<KeyEvent> RewriteReleaseKeyEvent(const KeyEvent& event);

 private:
  std::unique_ptr<KeyEvent> BuildRewrittenEvent(
      const KeyEvent& event,
      const RemappedKey& remapped_key);
  int RewriteModifierFlags(int flags) const;

  const raw_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  const raw_ptr<KeyboardCapability> keyboard_capability_;
  const raw_ptr<ash::input_method::ImeKeyboard> ime_keyboard_;

  // Tracks any keys that are remapped so they are consistently remapped until
  // after release.
  base::flat_map<internal::PhysicalKey, RemappedKey> remapped_keys_;
  // Tracks any remapped keys that are remapped to modifiers (capslock in this
  // case) and their corresponding flag.
  base::flat_map<internal::PhysicalKey, EventFlags> pressed_modifier_keys_;
};

}  // namespace ui

#endif  // UI_EVENTS_ASH_CAPS_LOCK_EVENT_REWRITER_H_
