// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_IME_KEYBOARD_H_
#define UI_BASE_IME_CHROMEOS_IME_KEYBOARD_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/observer_list.h"

namespace chromeos {
namespace input_method {

struct AutoRepeatRate {
  AutoRepeatRate() : initial_delay_in_ms(0), repeat_interval_in_ms(0) {}
  unsigned int initial_delay_in_ms;
  unsigned int repeat_interval_in_ms;
};

class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) ImeKeyboard {
 public:
  class Observer {
   public:
    // Called when the caps lock state has changed.
    virtual void OnCapsLockChanged(bool enabled) = 0;

    // Called when the layout state is changing.
    virtual void OnLayoutChanging(const std::string& layout_name) = 0;
  };

  ImeKeyboard();
  virtual ~ImeKeyboard();
  // Adds/removes observer.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Sets the current keyboard layout to |layout_name|. This function does not
  // change the current mapping of the modifier keys. Returns true on success.
  virtual bool SetCurrentKeyboardLayoutByName(const std::string& layout_name);

  // Gets the current keyboard layout name.
  const std::string& GetCurrentKeyboardLayoutName() const {
    return last_layout_;
  }

  // Sets the current keyboard layout again. We have to call the function every
  // time when "XI_HierarchyChanged" XInput2 event is sent to Chrome. See
  // xinput_hierarchy_changed_event_listener.h for details.
  virtual bool ReapplyCurrentKeyboardLayout() = 0;

  // Updates keyboard LEDs on all keyboards.
  // XKB asymmetrically propagates keyboard modifier indicator state changes to
  // slave keyboards. If the state change is initiated from a client to the
  // "core/master keyboard", XKB changes global state and pushes an indication
  // change down to all keyboards. If the state change is initiated by one slave
  // (physical) keyboard, it changes global state but only pushes an indicator
  // state change down to that one keyboard.
  // This function changes LEDs on all keyboards by explicitly updating the
  // core/master keyboard.
  virtual void ReapplyCurrentModifierLockStatus() = 0;

  // Disables the num lock.
  virtual void DisableNumLock() = 0;

  // Sets the caps lock status to |enable_caps_lock|. Do not call the function
  // from non-UI threads.
  virtual void SetCapsLockEnabled(bool enable_caps_lock);

  // Returns true if caps lock is enabled. Do not call the function from non-UI
  // threads.
  virtual bool CapsLockIsEnabled();

  // Returns true if the current layout supports ISO Level 5 shift.
  virtual bool IsISOLevel5ShiftAvailable() const;

  // Returns true if the current layout supports alt gr.
  virtual bool IsAltGrAvailable() const;

  // Turns on and off the auto-repeat of the keyboard. Returns true on success.
  // Do not call the function from non-UI threads.
  virtual bool SetAutoRepeatEnabled(bool enabled) = 0;

  // Returns true if auto-repeat is enabled.
  virtual bool GetAutoRepeatEnabled() = 0;

  // Sets the auto-repeat rate of the keyboard, initial delay in ms, and repeat
  // interval in ms.  Returns true on success. Do not call the function from
  // non-UI threads.
  virtual bool SetAutoRepeatRate(const AutoRepeatRate& rate) = 0;

  bool caps_lock_is_enabled_;
  std::string last_layout_;

 private:
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace input_method
}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_IME_KEYBOARD_H_
