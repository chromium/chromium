// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class describe a keyboard accelerator (or keyboard shortcut).
// Keyboard accelerators are registered with the FocusManager.
// It has a copy constructor and assignment operator so that it can be copied.
// It also defines the < operator so that it can be used as a key in a std::map.
//

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_H_

#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/base/ui_base_export.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

class KeyEvent;

// While |modifiers| may include EF_IS_REPEAT, EF_IS_REPEAT is not considered
// an intrinsic part of an Accelerator. This is done so that an accelerator
// for a particular KeyEvent matches an accelerator with or without the repeat
// flag. A side effect of this is that == (and <) does not consider the
// repeat flag in its comparison.
class UI_BASE_EXPORT Accelerator {
 public:
  enum class KeyState {
    PRESSED,
    RELEASED,
  };

  Accelerator();
  // |modifiers| consists of ui::EventFlags bitwise-or-ed together,
  // for example:
  //     Accelerator(ui::VKEY_Z, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)
  // would correspond to the shortcut "ctrl + shift + z".
  //
  // NOTE: this constructor strips out non key related flags.
  Accelerator(KeyboardCode key_code,
              int modifiers,
              KeyState key_state = KeyState::PRESSED,
              base::TimeTicks time_stamp = base::TimeTicks());
  explicit Accelerator(const KeyEvent& key_event);
  Accelerator(const Accelerator& accelerator);
  ~Accelerator();

  // Masks out all the non-modifiers KeyEvent |flags| and returns only the
  // available modifier ones. This does not include EF_IS_REPEAT.
  static int MaskOutKeyEventFlags(int flags);

  KeyEvent ToKeyEvent() const;

  Accelerator& operator=(const Accelerator& accelerator);

  // Define the < operator so that the KeyboardShortcut can be used as a key in
  // a std::map.
  bool operator <(const Accelerator& rhs) const;

  bool operator ==(const Accelerator& rhs) const;

  bool operator !=(const Accelerator& rhs) const;

  KeyboardCode key_code() const { return key_code_; }

  // Sets the key state that triggers the accelerator. Default is PRESSED.
  void set_key_state(KeyState state) { key_state_ = state; }
  KeyState key_state() const { return key_state_; }

  int modifiers() const { return modifiers_; }

  base::TimeTicks time_stamp() const { return time_stamp_; }

  int source_device_id() const { return source_device_id_; }

  bool IsShiftDown() const;
  bool IsCtrlDown() const;
  bool IsAltDown() const;
  bool IsCmdDown() const;
  bool IsRepeat() const;

  // Returns a string with the localized shortcut if any.
  base::string16 GetShortcutText() const;

#if defined(OS_MACOSX)
  base::string16 KeyCodeToMacSymbol() const;
#endif
  base::string16 KeyCodeToName() const;

  void set_interrupted_by_mouse_event(bool interrupted_by_mouse_event) {
    interrupted_by_mouse_event_ = interrupted_by_mouse_event;
  }

  bool interrupted_by_mouse_event() const {
    return interrupted_by_mouse_event_;
  }

 private:
  base::string16 ApplyLongFormModifiers(base::string16 shortcut) const;
  base::string16 ApplyShortFormModifiers(base::string16 shortcut) const;

  // The keycode (VK_...).
  KeyboardCode key_code_;

  KeyState key_state_;

  // The state of the Shift/Ctrl/Alt keys. This corresponds to Event::flags().
  int modifiers_;

  // The |time_stamp_| of the KeyEvent.
  base::TimeTicks time_stamp_;

  // Whether the accelerator is interrupted by a mouse press/release. This is
  // optionally used by AcceleratorController. Even this is set to true, the
  // accelerator may still be handled successfully. (Currently only
  // TOGGLE_APP_LIST and TOGGLE_APP_LIST_FULLSCREEN are disabled when mouse
  // press/release occurs between search key down and up. See crbug.com/665897)
  bool interrupted_by_mouse_event_;

  // The |source_device_id_| of the KeyEvent.
  int source_device_id_ = ui::ED_UNKNOWN_DEVICE;
};

// An interface that classes that want to register for keyboard accelerators
// should implement.
class UI_BASE_EXPORT AcceleratorTarget {
 public:
  // Should return true if the accelerator was processed.
  virtual bool AcceleratorPressed(const Accelerator& accelerator) = 0;

  // Should return true if the target can handle the accelerator events. The
  // AcceleratorPressed method is invoked only for targets for which
  // CanHandleAccelerators returns true.
  virtual bool CanHandleAccelerators() const = 0;

 protected:
  virtual ~AcceleratorTarget() {}
};

// Since accelerator code is one of the few things that can't be cross platform
// in the chrome UI, separate out just the GetAcceleratorForCommandId() from
// the menu delegates.
class AcceleratorProvider {
 public:
  // Gets the accelerator for the specified command id. Returns true if the
  // command id has a valid accelerator, false otherwise.
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          Accelerator* accelerator) const = 0;

 protected:
  virtual ~AcceleratorProvider() {}
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_H_
