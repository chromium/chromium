// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_KEYBOARD_LAYOUT_ENGINE_H_
#define UI_EVENTS_OZONE_LAYOUT_KEYBOARD_LAYOUT_ENGINE_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

enum class DomCode : uint32_t;

// A KeyboardLayoutEngine provides a platform-independent interface to
// key mapping. Key mapping provides a meaning (DomKey and character,
// and optionally Windows key code) for a physical key press (DomCode
// and modifier flags).
//
// This interface does not expose individual layouts because it must support
// platforms that only provide for one active system layout, and/or platforms
// where layouts have no accessible representation.
class COMPONENT_EXPORT(EVENTS_OZONE_LAYOUT) KeyboardLayoutEngine {
 public:
  KeyboardLayoutEngine() {}
  virtual ~KeyboardLayoutEngine() {}

  // Returns the current layout name.
  virtual std::string_view GetLayoutName() const = 0;

  // Returns true if it is possible to change the current layout.
  virtual bool CanSetCurrentLayout() const = 0;

  // Sets the current layout; returns true on success.
  // Drop-in replacement for ImeKeyboard::SetCurrentKeyboardLayoutByName();
  // the argument string is defined by that interface (crbug.com/362698).
  // Calls the callback once the layout is initialized after being set.
  virtual void SetCurrentLayoutByName(
      const std::string& layout_name,
      base::OnceCallback<void(bool)> callback) = 0;

  // Sets the current layout given a memory location and the buffer size in
  // bytes, that represent keyboard mapping description; returns true on
  // success.
  virtual bool SetCurrentLayoutFromBuffer(const char* keymap_string,
                                          size_t size) = 0;

  // Returns true if the current layout makes use of the ISO Level 5 Shift key.
  // Drop-in replacement for ImeKeyboard::IsISOLevel5ShiftAvailable().
  virtual bool UsesISOLevel5Shift() const = 0;

  // Returns true if the current layout makes use of the AltGr
  // (ISO Level 3 Shift) key.
  // Drop-in replacement for ImeKeyboard::IsAltGrAvailable().
  virtual bool UsesAltGr() const = 0;

  // Provides the meaning of a physical key.
  //
  // The caller must supply valid addresses for all the output parameters;
  // the function must not use their initial values.
  //
  // Returns true if it can determine the DOM meaning (i.e. ui::DomKey and
  // character) and the corresponding (non-located) KeyboardCode from the given
  // physical state (ui::DomCode and ui::EventFlags), OR if it can determine
  // that there is no meaning in the current layout (e.g. the key is unbound).
  // In the latter case, the function sets *dom_key to UNIDENTIFIED and
  // *key_code to VKEY_UNKNOWN.
  //
  // Returns false if it cannot determine the meaning (and cannot determine
  // that there is none); in this case it does not set any of the output
  // parameters.
  virtual bool Lookup(DomCode dom_code,
                      int event_flags,
                      DomKey* dom_key,
                      KeyboardCode* key_code) const = 0;

  // Tests may need to wait for the keyboard layout to be fully initialised.
  // The implementation should run |closure| when it is ready to handle calls to
  // Lookup().
  virtual void SetInitCallbackForTest(base::OnceClosure closure) = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_KEYBOARD_LAYOUT_ENGINE_H_
