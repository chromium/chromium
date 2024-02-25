// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_IME_KEYBOARD_H_
#define UI_BASE_IME_ASH_IME_KEYBOARD_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/time/time.h"

namespace ash {
namespace input_method {

struct AutoRepeatRate {
  base::TimeDelta initial_delay;
  base::TimeDelta repeat_interval;
};

class COMPONENT_EXPORT(UI_BASE_IME_ASH) ImeKeyboard {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the caps lock state has changed.
    virtual void OnCapsLockChanged(bool enabled) = 0;

    // Called when the layout state is changing.
    virtual void OnLayoutChanging(const std::string& layout_name) = 0;

   protected:
    ~Observer() override = default;
  };

  ImeKeyboard();
  virtual ~ImeKeyboard();

  // Adds/removes observer.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Sets the current keyboard layout to |layout_name|. This function does not
  // change the current mapping of the modifier keys. Callback is supplied
  // boolean with true on success and false on failure. Callback can potentially
  // be ran immediately with no delay.
  virtual void SetCurrentKeyboardLayoutByName(
      const std::string& layout_name,
      base::OnceCallback<void(bool)> callback);

  // Gets the current keyboard layout name.
  const std::string& GetCurrentKeyboardLayoutName() const {
    return last_layout_;
  }

  // Sets the caps lock status to |enable_caps_lock|. Do not call the function
  // from non-UI threads.
  virtual void SetCapsLockEnabled(bool enable_caps_lock);

  // Returns true if caps lock is enabled. Do not call the function from non-UI
  // threads.
  virtual bool IsCapsLockEnabled();

  // Returns true if the current layout supports ISO Level 5 shift.
  virtual bool IsISOLevel5ShiftAvailable() const;

  // Returns true if the current layout supports alt gr.
  virtual bool IsAltGrAvailable() const;

  // Turns on and off the auto-repeat of the keyboard.
  // Do not call the function from non-UI threads.
  virtual void SetAutoRepeatEnabled(bool enabled) = 0;

  // Returns true if auto-repeat is enabled.
  virtual bool GetAutoRepeatEnabled() = 0;

  // Sets the auto-repeat rate of the keyboard, initial delay in ms, and repeat
  // interval in ms.  Returns true on success. Do not call the function from
  // non-UI threads.
  virtual bool SetAutoRepeatRate(const AutoRepeatRate& rate) = 0;

 protected:
  bool SetCurrentKeyboardLayoutByNameImpl(const std::string& layout_name);

 private:
  bool caps_lock_is_enabled_ = false;
  std::string last_layout_;
  base::ObserverList<Observer> observers_;
};

}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_ASH_IME_KEYBOARD_H_
