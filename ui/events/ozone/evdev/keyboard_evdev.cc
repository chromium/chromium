// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/keyboard_evdev.h"

#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/types/event_type.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace ui {

namespace {

std::optional<KeyboardCode> RemapButtonsToKeyboardCodes(unsigned int key) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!ash::features::IsPeripheralCustomizationEnabled()) {
    return std::nullopt;
  }

  if (key >= BTN_0 && key <= BTN_9) {
    return static_cast<KeyboardCode>(static_cast<int>(VKEY_BUTTON_0) +
                                     (key - BTN_0));
  }

  // BTN_A through BTN_Z is only 6 buttons as it only includes {A, B, C, X, Y,
  // Z}.
  if (key >= BTN_A && key <= BTN_Z) {
    return static_cast<KeyboardCode>(static_cast<int>(VKEY_BUTTON_A) +
                                     (key - BTN_A));
  }
#endif

  return std::nullopt;
}

}  // namespace

KeyboardEvdev::KeyboardEvdev(
    EventModifiers* modifiers,
    KeyboardLayoutEngine* keyboard_layout_engine,
    const EventDispatchCallback& callback,
    base::RepeatingCallback<void(bool)> any_keys_are_pressed_callback)
    : callback_(callback),
      any_keys_are_pressed_callback_(any_keys_are_pressed_callback),
      modifiers_(modifiers),
      keyboard_layout_engine_(keyboard_layout_engine),
      auto_repeat_handler_(this) {}

KeyboardEvdev::~KeyboardEvdev() {
}

void KeyboardEvdev::OnKeyChange(unsigned int key,
                                unsigned int scan_code,
                                bool down,
                                bool suppress_auto_repeat,
                                base::TimeTicks timestamp,
                                int device_id,
                                int flags) {
  if (key > KEY_MAX)
    return;

  bool was_down = key_state_.test(key);
  bool is_repeat = down && was_down;
  if (!down && !was_down)
    return;  // Key already released.

  key_state_.set(key, down);
  // HID codes that are unknown to Linux can sometimes map to BTN_MISC. This can
  // cause BTN_MISC to appear permanently held down for some set of peripherals.
  // BTN_MISC is equal to BTN_0 which is seen on some graphics tablets, but for
  // the current usages of this check, graphics tablets holding a button down is
  // irrelevant. See b/331482962 for more info.
  {
    auto key_state_without_btn_misc_ = key_state_;
    key_state_without_btn_misc_.reset(BTN_MISC);
    any_keys_are_pressed_callback_.Run(key_state_without_btn_misc_.any());
  }
  auto_repeat_handler_.UpdateKeyRepeat(
      key, scan_code, down, suppress_auto_repeat, device_id, timestamp);
  DispatchKey(key, scan_code, down, is_repeat, timestamp, device_id, flags);
}

void KeyboardEvdev::SetCapsLockEnabled(bool enabled) {
  modifiers_->SetModifierLock(MODIFIER_CAPS_LOCK, enabled);
}

bool KeyboardEvdev::IsCapsLockEnabled() {
  return (modifiers_->GetModifierFlags() & EF_CAPS_LOCK_ON) != 0;
}

bool KeyboardEvdev::IsAutoRepeatEnabled() {
  return auto_repeat_handler_.IsAutoRepeatEnabled();
}

void KeyboardEvdev::SetAutoRepeatEnabled(bool enabled) {
  auto_repeat_handler_.SetAutoRepeatEnabled(enabled);
}

void KeyboardEvdev::SetAutoRepeatRate(const base::TimeDelta& delay,
                                      const base::TimeDelta& interval) {
  auto_repeat_handler_.SetAutoRepeatRate(delay, interval);
}

void KeyboardEvdev::GetAutoRepeatRate(base::TimeDelta* delay,
                                      base::TimeDelta* interval) {
  auto_repeat_handler_.GetAutoRepeatRate(delay, interval);
}

void KeyboardEvdev::SetCurrentLayoutByName(
    const std::string& layout_name,
    base::OnceCallback<void(bool)> callback) {
  keyboard_layout_engine_->SetCurrentLayoutByName(layout_name,
                                                  std::move(callback));
  RefreshModifiers();
}

void KeyboardEvdev::FlushInput(base::OnceClosure closure) {
  // Post a task behind any pending key releases in the message loop
  // FIFO. This ensures there's no spurious repeats during periods of UI
  // thread jank.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(closure));
}

void KeyboardEvdev::UpdateModifier(int modifier_flag, bool down) {
  if (modifier_flag == EF_NONE)
    return;

  int modifier = EventModifiers::GetModifierFromEventFlag(modifier_flag);
  if (modifier == MODIFIER_NONE)
    return;

  // TODO post-X11: Revise remapping to not use EF_MOD3_DOWN.
  // Currently EF_MOD3_DOWN means that the CapsLock key is currently down,
  // and EF_CAPS_LOCK_ON means the caps lock state is enabled (and the
  // key may or may not be down, but usually isn't). There does need to
  // to be two different flags, since the physical CapsLock key is subject
  // to remapping, but the caps lock state (which can be triggered in a
  // variety of ways) is not.
  if (modifier == MODIFIER_CAPS_LOCK)
    modifiers_->UpdateModifier(MODIFIER_MOD3, down);
  else
    modifiers_->UpdateModifier(modifier, down);
}

void KeyboardEvdev::RefreshModifiers() {
  // Release all keyboard modifiers.
  modifiers_->ResetKeyboardModifiers();
  // Press modifiers for currently held keys.
  for (int key = 0; key < KEY_CNT; ++key) {
    if (!key_state_.test(key))
      continue;
    DomCode dom_code = KeycodeConverter::EvdevCodeToDomCode(key);
    if (dom_code == DomCode::NONE)
      continue;
    DomKey dom_key;
    KeyboardCode keycode;
    if (!keyboard_layout_engine_->Lookup(dom_code, EF_NONE, &dom_key, &keycode))
      continue;
    int flag = ModifierDomKeyToEventFlag(dom_key);
    if (flag == EF_NONE)
      continue;
    UpdateModifier(flag, true);
  }
}

void KeyboardEvdev::DispatchKey(unsigned int key,
                                unsigned int scan_code,
                                bool down,
                                bool repeat,
                                base::TimeTicks timestamp,
                                int device_id,
                                int flags) {
  DomCode dom_code = KeycodeConverter::EvdevCodeToDomCode(key);
  DomKey dom_key;
  KeyboardCode key_code;

  if (auto button_key_code = RemapButtonsToKeyboardCodes(key);
      button_key_code.has_value()) {
    dom_code = DomCode::NONE;
    dom_key = DomKey::NONE;
    key_code = *button_key_code;
  } else {
    if (dom_code == DomCode::NONE) {
      return;
    }
    int modifier_flags = modifiers_->GetModifierFlags();
    if (!keyboard_layout_engine_->Lookup(dom_code, modifier_flags, &dom_key,
                                         &key_code)) {
      return;
    }
    if (!repeat) {
      int flag = ModifierDomKeyToEventFlag(dom_key);
      UpdateModifier(flag, down);
    }
  }

  KeyEvent event(down ? EventType::kKeyPressed : EventType::kKeyReleased,
                 key_code, dom_code, flags | modifiers_->GetModifierFlags(),
                 dom_key, timestamp);
  event.set_scan_code(scan_code);
  event.set_source_device_id(device_id);
  callback_.Run(&event);
}

}  // namespace ui
