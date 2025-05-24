// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class describe a keyboard accelerator (or keyboard shortcut).
// Keyboard accelerators are registered with the FocusManager.
// It has a copy constructor and assignment operator so that it can be copied.
// It also defines the < operator so that it can be used as a key in a std::map.

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_H_

#include <compare>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/events/keycodes/dom/dom_code.h"
#endif

namespace ui {

class KeyEvent;

// While |modifiers| may include EF_IS_REPEAT, EF_IS_REPEAT is not considered
// an intrinsic part of an Accelerator. This is done so that an accelerator
// for a particular KeyEvent matches an accelerator with or without the repeat
// flag. A side effect of this is that == (and <) does not consider the
// repeat flag in its comparison.
class COMPONENT_EXPORT(UI_BASE) Accelerator {
 private:
  static constexpr int kModifierMask = EF_SHIFT_DOWN | EF_CONTROL_DOWN |
                                       EF_ALT_DOWN | EF_COMMAND_DOWN |
                                       EF_FUNCTION_DOWN | EF_ALTGR_DOWN;

  static constexpr int kInterestingFlagsMask =
      kModifierMask | EF_IS_SYNTHESIZED | EF_IS_REPEAT;

 public:
  enum class KeyState {
    PRESSED,
    RELEASED,
  };

  constexpr Accelerator() : Accelerator(VKEY_UNKNOWN, EF_NONE) {}
  // |modifiers| consists of ui::EventFlags bitwise-or-ed together,
  // for example:
  //     Accelerator(ui::VKEY_Z, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)
  // would correspond to the shortcut "ctrl + shift + z".
  constexpr Accelerator(KeyboardCode key_code,
                        int modifiers,
                        KeyState key_state = KeyState::PRESSED,
                        base::TimeTicks time_stamp = base::TimeTicks())
      : key_code_(key_code),
        key_state_(key_state),
        modifiers_(modifiers & kInterestingFlagsMask),
        time_stamp_(time_stamp) {}

#if BUILDFLAG(IS_CHROMEOS)
  // Additional constructor that takes a |DomCode| in order to implement
  // layout independent fixed position shortcuts. This is only used for
  // shortcuts in Chrome OS. One such example is Alt ']'. In the US layout ']'
  // is VKEY_OEM_6, in the DE layout it is VKEY_OEM_PLUS. However the key in
  // that position is always DomCode::BRACKET_RIGHT regardless of what the key
  // generates when pressed. When the DE layout is used and the accelerator
  // is created with { VKEY_OEM_PLUS, DomCode::BRACKET_RIGHT } the custom
  // accelerator map will map BRACKET_RIGHT to VKEY_OEM_6 as if in the US
  // layout in order to lookup the accelerator.
  //
  // See accelerator_map.h for more information.
  constexpr Accelerator(KeyboardCode key_code,
                        DomCode code,
                        int modifiers,
                        KeyState key_state = KeyState::PRESSED,
                        base::TimeTicks time_stamp = base::TimeTicks())
      : key_code_(key_code),
        code_(code),
        key_state_(key_state),
        modifiers_(modifiers & kInterestingFlagsMask),
        time_stamp_(time_stamp) {}
#endif

  explicit Accelerator(const KeyEvent& key_event);
  constexpr Accelerator(const Accelerator& accelerator) = default;
  constexpr Accelerator& operator=(const Accelerator& accelerator) = default;
  constexpr ~Accelerator() = default;

  // Masks out all the non-modifiers KeyEvent |flags| and returns only the
  // available modifier ones. This does not include EF_IS_REPEAT.
  static constexpr int MaskOutKeyEventFlags(int flags) {
    return flags & kModifierMask;
  }

  KeyEvent ToKeyEvent() const;

  constexpr bool operator==(const Accelerator& rhs) const {
    return (key_code_ == rhs.key_code_) && (key_state_ == rhs.key_state_) &&
           (MaskOutKeyEventFlags(modifiers_) ==
            MaskOutKeyEventFlags(rhs.modifiers_)) &&
           interrupted_by_mouse_event_ == rhs.interrupted_by_mouse_event_;
  }

  constexpr auto operator<=>(const Accelerator& rhs) const {
    const int modifiers_with_mask = MaskOutKeyEventFlags(modifiers_);
    const int rhs_modifiers_with_mask = MaskOutKeyEventFlags(rhs.modifiers_);
    return std::tie(key_code_, key_state_, modifiers_with_mask) <=>
           std::tie(rhs.key_code_, rhs.key_state_, rhs_modifiers_with_mask);
  }

  constexpr KeyboardCode key_code() const { return key_code_; }

  constexpr bool IsEmpty() const {
    return key_code_ == VKEY_UNKNOWN && modifiers_ == EF_NONE;
  }

#if BUILDFLAG(IS_CHROMEOS)
  DomCode code() const { return code_; }
  void reset_code() { code_ = DomCode::NONE; }
#endif

  // Sets the key state that triggers the accelerator. Default is PRESSED.
  void set_key_state(KeyState state) { key_state_ = state; }
  KeyState key_state() const { return key_state_; }

  int modifiers() const { return modifiers_; }

  base::TimeTicks time_stamp() const { return time_stamp_; }

  int source_device_id() const { return source_device_id_; }

  constexpr bool IsShiftDown() const {
    return (modifiers_ & EF_SHIFT_DOWN) != 0;
  }

  constexpr bool IsCtrlDown() const {
    return (modifiers_ & EF_CONTROL_DOWN) != 0;
  }

  constexpr bool IsAltDown() const { return (modifiers_ & EF_ALT_DOWN) != 0; }

  constexpr bool IsAltGrDown() const {
    return (modifiers_ & EF_ALTGR_DOWN) != 0;
  }

  constexpr bool IsCmdDown() const {
    return (modifiers_ & EF_COMMAND_DOWN) != 0;
  }

  constexpr bool IsFunctionDown() const {
    return (modifiers_ & EF_FUNCTION_DOWN) != 0;
  }

  constexpr bool IsRepeat() const { return (modifiers_ & EF_IS_REPEAT) != 0; }

#if BUILDFLAG(USE_BLINK)
  // Returns true if the specified accelerator is one of the following
  // multimedia keys: Next Track key, Previous Track key, Stop Media key,
  // Play/Pause Media key, without any modifiers.
  bool IsMediaKey() const;
#endif

  // Returns a string with the localized shortcut if any.
  std::u16string GetShortcutText() const;

  // Returns a vector representation of the localized shortcut. For example, for
  // a shortcut "Control + Shift + A", this would return ["Ctrl", "Shift", "A"].
  std::vector<std::u16string> GetShortcutVectorRepresentation() const;

#if BUILDFLAG(IS_MAC)
  std::u16string KeyCodeToMacSymbol() const;
#endif
  std::u16string KeyCodeToName() const;

  void set_interrupted_by_mouse_event(bool interrupted_by_mouse_event) {
    interrupted_by_mouse_event_ = interrupted_by_mouse_event;
  }

  bool interrupted_by_mouse_event() const {
    return interrupted_by_mouse_event_;
  }

 private:
  friend class AcceleratorTestMac;
  std::vector<std::u16string> GetLongFormModifiers() const;
  std::vector<std::u16string> GetShortFormModifiers() const;
  std::u16string GetKeyCodeStringForShortcut() const;

  // The keycode (VK_...).
  KeyboardCode key_code_;

#if BUILDFLAG(IS_CHROMEOS)
  // The DomCode representing a key's physical position.
  DomCode code_ = DomCode::NONE;
#endif

  KeyState key_state_;

  // The state of the Shift/Ctrl/Alt keys. This corresponds to Event::flags().
  int modifiers_;

  // The |time_stamp_| of the KeyEvent.
  base::TimeTicks time_stamp_;

  // Whether the accelerator is interrupted by a mouse press/release. This is
  // optionally used by AcceleratorController. Even this is set to true, the
  // accelerator may still be handled successfully. (Currently only
  // AcceleratorAction::kToggleAppList is disabled when mouse press/release
  // occurs between search key down and up. See crbug.com/665897)
  bool interrupted_by_mouse_event_ = false;

  // The |source_device_id_| of the KeyEvent.
  int source_device_id_ = ui::ED_UNKNOWN_DEVICE;
};

// An interface that classes that want to register for keyboard accelerators
// should implement.
class COMPONENT_EXPORT(UI_BASE) AcceleratorTarget {
 public:
  // Should return true if the accelerator was processed.
  virtual bool AcceleratorPressed(const Accelerator& accelerator) = 0;

  // Should return true if the target can handle the accelerator events. The
  // AcceleratorPressed method is invoked only for targets for which
  // CanHandleAccelerators returns true.
  virtual bool CanHandleAccelerators() const = 0;

 protected:
  virtual ~AcceleratorTarget() = default;
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
  virtual ~AcceleratorProvider() = default;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_H_
