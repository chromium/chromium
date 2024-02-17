// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_KEYBOARD_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_KEYBOARD_EVDEV_H_

#include <linux/input.h>

#include <bitset>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/events/ozone/evdev/event_device_util.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/events/ozone/keyboard/event_auto_repeat_handler.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"

namespace ui {

class EventModifiers;
enum class DomCode : uint32_t;

// Keyboard for evdev.
//
// This object is responsible for combining all attached keyboards into
// one logical keyboard, applying modifiers & implementing key repeat.
//
// It also currently also applies the layout.
class COMPONENT_EXPORT(EVDEV) KeyboardEvdev
    : public EventAutoRepeatHandler::Delegate {
 public:
  KeyboardEvdev(
      EventModifiers* modifiers,
      KeyboardLayoutEngine* keyboard_layout_engine,
      const EventDispatchCallback& callback,
      base::RepeatingCallback<void(bool)> any_keys_are_pressed_callback);

  KeyboardEvdev(const KeyboardEvdev&) = delete;
  KeyboardEvdev& operator=(const KeyboardEvdev&) = delete;

  ~KeyboardEvdev();

  // Handlers for raw key presses & releases.
  //
  // |code| is a Linux key code (from <linux/input.h>). |scan_code| is a
  // device dependent code that identifies the physical location of the key
  // independent of it's mapping to a linux key code. |down| represents the
  // key state. |suppress_auto_repeat| prevents the event from triggering
  // auto-repeat, if enabled. |device_id| uniquely identifies the source
  // keyboard device.
  void OnKeyChange(unsigned int code,
                   unsigned int scan_code,
                   bool down,
                   bool suppress_auto_repeat,
                   base::TimeTicks timestamp,
                   int device_id,
                   int flags);

  // Handle Caps Lock modifier.
  void SetCapsLockEnabled(bool enabled);
  bool IsCapsLockEnabled();

  // Configuration for key repeat.
  bool IsAutoRepeatEnabled();
  void SetAutoRepeatEnabled(bool enabled);
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval);
  void GetAutoRepeatRate(base::TimeDelta* delay, base::TimeDelta* interval);

  // Handle keyboard layout changes.
  void SetCurrentLayoutByName(const std::string& layout_name,
                              base::OnceCallback<void(bool)> callback);

 private:
  void UpdateModifier(int modifier_flag, bool down);
  void RefreshModifiers();
  void UpdateCapsLockLed();

  // EventAutoRepeatHandler::Delegate
  void FlushInput(base::OnceClosure closure) override;
  void DispatchKey(unsigned int key,
                   unsigned int scan_code,
                   bool down,
                   bool repeat,
                   base::TimeTicks timestamp,
                   int device_id,
                   int flags) override;

  // Aggregated key state. There is only one bit of state per key; we do not
  // attempt to count presses of the same key on multiple keyboards.
  //
  // A key is down iff the most recent event pertaining to that key was a key
  // down event rather than a key up event. Therefore, a particular key position
  // can be considered released even if it is being depresssed on one or more
  // keyboards.
  std::bitset<KEY_CNT> key_state_;

  // Callback for dispatching events.
  const EventDispatchCallback callback_;

  const base::RepeatingCallback<void(bool)> any_keys_are_pressed_callback_;

  // Shared modifier state.
  const raw_ptr<EventModifiers> modifiers_;

  // Shared layout engine.
  const raw_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;

  // Key repeat handler.
  EventAutoRepeatHandler auto_repeat_handler_;

  base::WeakPtrFactory<KeyboardEvdev> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_KEYBOARD_EVDEV_H_
